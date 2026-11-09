/*
 * HyPocket direct display for the VirGL server.
 * Copyright (C) 2026 HyPocket Launcher contributors. GPL-3.0-or-later.
 *
 * The app listens on an abstract Unix socket (HYPOCKET_DISPLAY_SOCKET) and
 * draws the AHardwareBuffers this server sends it, so frames never go
 * through the CPU. There are two kinds of buffers:
 *
 * - Adopted (ids 3 and up): the guest's Mesa asks us to adopt each pbuffer
 *   image it will present (VCMD_HYPOCKET_ADOPT) right after creating it. We
 *   back that resource's texture with a new AHardwareBuffer, so the guest
 *   draws straight into it and a present only sends a fence: no copy.
 * - Copy (ids 0 to 2): for anything else presented (the first frame, or a
 *   format we cannot adopt), the guest's texture is copied into a free one
 *   of these on the GPU.
 *
 * Messages (little endian, 16 bytes): uint32 type, id, width, height.
 *   server -> app  1 BUFFER  followed by the AHardwareBuffer handle
 *   server -> app  2 FRAME   with the fence fd (if any) attached
 *   app -> server  3 RELEASE the buffer is no longer on screen
 *   server -> app  4 FORGET  the buffer is gone
 */
#include "hypocket_display.h"

#ifdef __ANDROID__

#include <android/hardware_buffer.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "virglrenderer.h"

/* Exported by our patches to src/vrend/vrend_winsys_egl.c and vrend_renderer.c. */
bool hypocket_egl_get(EGLDisplay *display, EGLConfig *config, EGLContext *context);
GLuint hypocket_vrend_back_with_image(uint32_t res_id, void *image);

#define BUFFERS 3     /* copy buffers, ids 0 to BUFFERS - 1 */
#define ALL_BUFFERS 8 /* adopted buffers use the ids after them */
/* How long a present may wait for the screen to give back an adopted buffer. */
#define RELEASE_WAIT_MS 50
enum { MSG_BUFFER = 1, MSG_FRAME = 2, MSG_RELEASE = 3, MSG_FORGET = 4 };

struct buffer {
   AHardwareBuffer *ahb;
   EGLImageKHR image;
   GLuint tex, fbo;
   bool busy;
   /* Adopted buffers only: the resource and its (new) texture. */
   uint32_t res_id, width, height;
};

static struct {
   int state; /* 0 not tried, 1 on, -1 off */
   int sock;
   bool skip_readback;
   bool has_wait_sync, has_native_fence;
   EGLDisplay dpy;
   EGLConfig config;
   EGLContext ctx;
   GLuint program, sampler, vao;
   GLint size_location, flip_location;
   unsigned logged_errors;
   uint32_t width, height;
   struct buffer buf[ALL_BUFFERS];
   unsigned frames, dropped;
   int last_adopted; /* id of the adopted buffer presented last, or -1 */
   unsigned adopted_frames, release_timeouts;
} d = { .sock = -1, .last_adopted = -1 };

static void log_msg(const char *msg)
{
   fprintf(stderr, "hypocket-display: %s\n", msg);
}

static bool send_all(const void *data, size_t size)
{
   const char *p = data;
   while (size) {
      ssize_t n = send(d.sock, p, size, MSG_NOSIGNAL);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0) return false;
      p += n;
      size -= (size_t)n;
   }
   return true;
}

static bool send_header(uint32_t type, uint32_t id, uint32_t width, uint32_t height, int fd)
{
   uint32_t msg[4] = { type, id, width, height };
   if (fd < 0) return send_all(msg, sizeof(msg));

   struct iovec iov = { msg, sizeof(msg) };
   char control[CMSG_SPACE(sizeof(int))];
   memset(control, 0, sizeof(control));
   struct msghdr mh = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = control,
                        .msg_controllen = sizeof(control) };
   struct cmsghdr *cmsg = CMSG_FIRSTHDR(&mh);
   cmsg->cmsg_level = SOL_SOCKET;
   cmsg->cmsg_type = SCM_RIGHTS;
   cmsg->cmsg_len = CMSG_LEN(sizeof(int));
   memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
   return sendmsg(d.sock, &mh, MSG_NOSIGNAL) == (ssize_t)sizeof(msg);
}

static void disable(const char *why)
{
   log_msg(why);
   if (d.sock >= 0) close(d.sock);
   d.sock = -1;
   d.state = -1;
}

/*
 * Copies with a shader instead of glBlitFramebuffer: a blit fails when the
 * guest's texture format differs from the buffer's (BGRA vs RGBA), and
 * sampling also applies the texture swizzle virglrenderer may use for BGRA.
 * texelFetch reads exact pixels; the y flip turns GL's bottom-first rows
 * into the screen's top-first rows.
 */
static const char *vertex_src =
   "#version 300 es\n"
   "void main() {\n"
   "   vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
   "   gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
   "}\n";
static const char *fragment_src =
   "#version 300 es\n"
   "precision mediump float;\n"
   "uniform sampler2D frame;\n"
   "uniform ivec2 size;\n"
   "uniform bool flip;\n"
   "out vec4 color;\n"
   "void main() {\n"
   "   ivec2 p = ivec2(gl_FragCoord.xy);\n"
   "   color = vec4(texelFetch(frame, ivec2(p.x, flip ? size.y - 1 - p.y : p.y), 0).rgb, 1.0);\n"
   "}\n";

static GLuint compile(GLenum type, const char *src)
{
   GLuint shader = glCreateShader(type);
   glShaderSource(shader, 1, &src, NULL);
   glCompileShader(shader);
   GLint ok = 0;
   glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
   if (!ok) {
      char text[512];
      glGetShaderInfoLog(shader, sizeof(text), NULL, text);
      fprintf(stderr, "hypocket-display: shader: %s\n", text);
      glDeleteShader(shader);
      return 0;
   }
   return shader;
}

/* Called with our context current. */
static bool make_program(void)
{
   GLuint vs = compile(GL_VERTEX_SHADER, vertex_src);
   GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_src);
   if (!vs || !fs) return false;
   d.program = glCreateProgram();
   glAttachShader(d.program, vs);
   glAttachShader(d.program, fs);
   glLinkProgram(d.program);
   glDeleteShader(vs);
   glDeleteShader(fs);
   GLint ok = 0;
   glGetProgramiv(d.program, GL_LINK_STATUS, &ok);
   if (!ok) return false;
   glUseProgram(d.program);
   glUniform1i(glGetUniformLocation(d.program, "frame"), 0);
   d.size_location = glGetUniformLocation(d.program, "size");
   d.flip_location = glGetUniformLocation(d.program, "flip");
   /* A sampler object makes the texture complete whatever its own filters say. */
   glGenSamplers(1, &d.sampler);
   glSamplerParameteri(d.sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glSamplerParameteri(d.sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glGenVertexArrays(1, &d.vao);
   return true;
}

static void check_gl(const char *where)
{
   GLenum err = glGetError();
   if (err != GL_NO_ERROR && d.logged_errors < 10) {
      d.logged_errors++;
      fprintf(stderr, "hypocket-display: GL error 0x%x at %s\n", err, where);
   }
}

static bool init(void)
{
   if (d.state) return d.state > 0;
   const char *name = getenv("HYPOCKET_DISPLAY_SOCKET");
   if (!name || !*name || strlen(name) + 1 >= sizeof(((struct sockaddr_un *)0)->sun_path)) {
      d.state = -1;
      return false;
   }
   d.skip_readback = getenv("HYPOCKET_SKIP_READBACK") != NULL;

   struct sockaddr_un addr;
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;
   /* Abstract namespace: a leading NUL byte. */
   memcpy(addr.sun_path + 1, name, strlen(name));
   socklen_t len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + strlen(name));
   d.sock = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
   if (d.sock < 0 || connect(d.sock, (struct sockaddr *)&addr, len) != 0) {
      disable("cannot connect to the app");
      return false;
   }

   EGLConfig config;
   EGLContext share;
   if (!hypocket_egl_get(&d.dpy, &config, &share)) {
      disable("no EGL display");
      return false;
   }
   /* libepoxy aborts when a missing function is called, so check first. */
   if (!epoxy_has_egl_extension(d.dpy, "EGL_ANDROID_get_native_client_buffer") ||
       !epoxy_has_egl_extension(d.dpy, "EGL_ANDROID_image_native_buffer") ||
       !epoxy_has_egl_extension(d.dpy, "EGL_KHR_fence_sync")) {
      disable("the driver lacks the EGL extensions for direct display");
      return false;
   }
   d.has_wait_sync = epoxy_has_egl_extension(d.dpy, "EGL_KHR_wait_sync");
   d.has_native_fence = epoxy_has_egl_extension(d.dpy, "EGL_ANDROID_native_fence_sync");
   (void)share;
   d.config = config;
   d.state = 1;
   log_msg("on");
   return true;
}

/*
 * Creates our context in the share group of the current context, which is
 * the guest's: virglrenderer's first context is not shared with the EGL
 * setup context, so texture names only mean the same thing in the group of
 * the contexts that render. Called with the guest's context current.
 */
static bool make_context(void)
{
   if (d.ctx != EGL_NO_CONTEXT) return true;
   EGLContext share = eglGetCurrentContext();
   EGLSurface draw = eglGetCurrentSurface(EGL_DRAW), read = eglGetCurrentSurface(EGL_READ);
   const EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   d.ctx = eglCreateContext(d.dpy, d.config, share, attribs);
   if (d.ctx == EGL_NO_CONTEXT) return false;
   eglMakeCurrent(d.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, d.ctx);
   bool ok = epoxy_has_gl_extension("GL_OES_EGL_image") && make_program();
   eglMakeCurrent(d.dpy, draw, read, share);
   return ok;
}

bool hypocket_display_enabled(void)
{
   return init();
}

bool hypocket_display_skip_readback(void)
{
   return init() && d.skip_readback;
}

static void free_buffers(void)
{
   /* Only the copy buffers: adopted ones live as long as their resource. */
   for (int i = 0; i < BUFFERS; i++) {
      struct buffer *b = &d.buf[i];
      if (b->fbo) glDeleteFramebuffers(1, &b->fbo);
      if (b->tex) glDeleteTextures(1, &b->tex);
      if (b->image) eglDestroyImageKHR(d.dpy, b->image);
      if (b->ahb) AHardwareBuffer_release(b->ahb);
      memset(b, 0, sizeof(*b));
   }
   d.width = d.height = 0;
}

/* Allocates the buffers for a new frame size and sends them to the app. */
static bool make_buffers(uint32_t width, uint32_t height)
{
   free_buffers();
   d.width = width;
   d.height = height;
   for (uint32_t i = 0; i < BUFFERS; i++) {
      struct buffer *b = &d.buf[i];
      AHardwareBuffer_Desc desc = {
         .width = width, .height = height, .layers = 1,
         .format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
         .usage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                  AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY,
      };
      if (AHardwareBuffer_allocate(&desc, &b->ahb) != 0) return false;
      EGLClientBuffer client = eglGetNativeClientBufferANDROID(b->ahb);
      const EGLint image_attribs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
      b->image = eglCreateImageKHR(d.dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, client, image_attribs);
      if (b->image == EGL_NO_IMAGE_KHR) return false;
      glGenTextures(1, &b->tex);
      glBindTexture(GL_TEXTURE_2D, b->tex);
      glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)b->image);
      glGenFramebuffers(1, &b->fbo);
      glBindFramebuffer(GL_FRAMEBUFFER, b->fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, b->tex, 0);
      GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE) {
         fprintf(stderr, "hypocket-display: buffer framebuffer incomplete 0x%x\n", status);
         return false;
      }
      if (!send_header(MSG_BUFFER, i, width, height, -1) ||
          AHardwareBuffer_sendHandleToUnixSocket(b->ahb, d.sock) != 0)
         return false;
   }
   return true;
}

/* Reads the app's RELEASE messages without waiting. */
static void read_releases(void)
{
   uint32_t msg[4];
   for (;;) {
      ssize_t n = recv(d.sock, msg, sizeof(msg), MSG_DONTWAIT | MSG_PEEK);
      if (n < (ssize_t)sizeof(msg)) {
         if (n == 0) disable("the app closed the display");
         return;
      }
      if (recv(d.sock, msg, sizeof(msg), MSG_WAITALL) != (ssize_t)sizeof(msg)) return;
      if (msg[0] == MSG_RELEASE && msg[1] < ALL_BUFFERS) d.buf[msg[1]].busy = false;
   }
}


/* The buffer format matching a resource's format, as virglrenderer stores
 * it on GLES (BGRA and 10-bit BGR formats are stored in RGBA order), or 0
 * when we do not adopt it. Formats without alpha are left out: for those
 * virglrenderer treats an EGL-image-backed texture as 24-bit. */
static uint32_t buffer_format(uint32_t virgl_format)
{
   switch (virgl_format) {
   case 1:   /* B8G8R8A8_UNORM */
   case 67:  /* R8G8B8A8_UNORM */
      return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
   case 131: /* B10G10R10A2_UNORM */
   case 233: /* B10G10R10X2_UNORM */
   case 8:   /* R10G10B10A2_UNORM */
   case 308: /* R10G10B10X2_UNORM */
      return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
   default:
      return 0;
   }
}

static void free_adopted(struct buffer *b)
{
   if (b->image) eglDestroyImageKHR(d.dpy, b->image);
   if (b->ahb) AHardwareBuffer_release(b->ahb);
   memset(b, 0, sizeof(*b));
}

void hypocket_display_adopt(uint32_t res_id)
{
   static unsigned refused;
   if (!init()) return;
   struct virgl_renderer_resource_info info;
   if (virgl_renderer_resource_get_info((int)res_id, &info) != 0 || !info.tex_id || info.depth > 1)
      return;
   uint32_t format = buffer_format(info.virgl_format);
   if (!format) {
      if (refused++ < 3)
         fprintf(stderr, "hypocket-display: not adopting resource %u: format %u\n", res_id, info.virgl_format);
      return;
   }
   int index = -1;
   for (int i = BUFFERS; i < ALL_BUFFERS; i++)
      if (!d.buf[i].ahb) { index = i; break; }
   if (index < 0) {
      if (refused++ < 3) log_msg("not adopting: no free buffer slot");
      return;
   }
   struct buffer *b = &d.buf[index];
   AHardwareBuffer_Desc desc = {
      .width = info.width, .height = info.height, .layers = 1, .format = format,
      .usage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
               AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY,
   };
   if (AHardwareBuffer_allocate(&desc, &b->ahb) != 0) {
      b->ahb = NULL;
      if (refused++ < 3)
         fprintf(stderr, "hypocket-display: not adopting resource %u: no %ux%u buffer of format %u\n", res_id,
                 info.width, info.height, format);
      return;
   }
   const EGLint image_attribs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
   b->image = eglCreateImageKHR(d.dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                                eglGetNativeClientBufferANDROID(b->ahb), image_attribs);
   b->tex = b->image != EGL_NO_IMAGE_KHR ? hypocket_vrend_back_with_image(res_id, b->image) : 0;
   if (!b->tex) {
      if (refused++ < 3) fprintf(stderr, "hypocket-display: could not back resource %u with a buffer\n", res_id);
      free_adopted(b);
      return;
   }
   b->res_id = res_id;
   b->width = info.width;
   b->height = info.height;
   if (!send_header(MSG_BUFFER, (uint32_t)index, b->width, b->height, -1) ||
       AHardwareBuffer_sendHandleToUnixSocket(b->ahb, d.sock) != 0) {
      disable("cannot send an adopted buffer");
      return;
   }
   static bool logged;
   if (!logged) {
      logged = true;
      fprintf(stderr, "hypocket-display: adopted resource %u, %ux%u format %u: frames need no copy\n", res_id,
              info.width, info.height, info.virgl_format);
   }
}

void hypocket_display_forget(uint32_t res_id)
{
   if (d.state <= 0) return;
   for (int i = BUFFERS; i < ALL_BUFFERS; i++) {
      struct buffer *b = &d.buf[i];
      if (!b->ahb || b->res_id != res_id) continue;
      if (d.sock >= 0) send_header(MSG_FORGET, (uint32_t)i, b->width, b->height, -1);
      free_adopted(b);
      if (d.last_adopted == i) d.last_adopted = -1;
   }
}

/* Once frames need no copy, the copy buffers only hold memory: free them
 * (they come back if a frame ever needs a copy again). Framebuffer objects
 * are not shared between contexts, so this needs our own context. */
static void free_copy_buffers(void)
{
   if (d.ctx == EGL_NO_CONTEXT || !d.buf[0].ahb) return;
   EGLContext prev_ctx = eglGetCurrentContext();
   EGLSurface prev_draw = eglGetCurrentSurface(EGL_DRAW);
   EGLSurface prev_read = eglGetCurrentSurface(EGL_READ);
   eglMakeCurrent(d.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, d.ctx);
   for (uint32_t i = 0; i < BUFFERS; i++)
      if (d.sock >= 0) send_header(MSG_FORGET, i, d.width, d.height, -1);
   free_buffers();
   eglMakeCurrent(d.dpy, prev_draw, prev_read, prev_ctx);
   log_msg("freed the copy buffers");
}

/* Waits until every adopted buffer except index and the one shown before it
 * is back from the screen: the guest draws into the oldest one next. With
 * HYPOCKET_DISPLAY_UNTHROTTLED (benchmarks) it does not wait: frames are not
 * held to the screen's refresh rate, at the cost of possible tearing. */
static void wait_for_releases(int index)
{
   static int unthrottled = -1;
   if (unthrottled < 0)
      unthrottled = getenv("HYPOCKET_DISPLAY_UNTHROTTLED") != NULL;
   if (unthrottled) {
      read_releases();
      return;
   }
   for (int waited = 0;;) {
      read_releases();
      if (d.state < 0) return;
      bool ready = true;
      for (int i = BUFFERS; i < ALL_BUFFERS; i++)
         if (d.buf[i].ahb && d.buf[i].busy && i != index && i != d.last_adopted) ready = false;
      if (ready) return;
      if (waited >= RELEASE_WAIT_MS) {
         if (d.release_timeouts++ < 3) log_msg("the screen kept a buffer too long; drawing anyway");
         return;
      }
      struct pollfd p = { d.sock, POLLIN, 0 };
      poll(&p, 1, 5);
      waited += 5;
   }
}

/* Presents an adopted buffer: the guest drew into it already, so only a
 * fence goes to the app. Called with the guest's context current. */
static void present_adopted(int index)
{
   struct buffer *b = &d.buf[index];
   int fence_fd = -1;
   EGLSyncKHR done = d.has_native_fence ? eglCreateSyncKHR(d.dpy, EGL_SYNC_NATIVE_FENCE_ANDROID, NULL)
                                        : EGL_NO_SYNC_KHR;
   glFlush();
   if (done != EGL_NO_SYNC_KHR) {
      fence_fd = eglDupNativeFenceFDANDROID(d.dpy, done);
      eglDestroySyncKHR(d.dpy, done);
   }
   if (fence_fd < 0) glFinish();
   wait_for_releases(index);
   if (d.state < 0) {
      if (fence_fd >= 0) close(fence_fd);
      return;
   }
   b->busy = true;
   if (!send_header(MSG_FRAME, (uint32_t)index, b->width, b->height, fence_fd)) disable("cannot send a frame");
   if (fence_fd >= 0) close(fence_fd);
   d.last_adopted = index;
   d.frames++;
   if (d.adopted_frames++ == 0) log_msg("first frame without a copy");
   if (d.adopted_frames == 2) free_copy_buffers();
}

bool hypocket_display_present(uint32_t res_id, uint32_t width, uint32_t height, bool y0_top)
{
   if (!init()) return false;

   /* An adopted pbuffer image: no copy needed. Its rows are stored top first,
    * like the copy buffers. */
   for (int i = BUFFERS; y0_top && i < ALL_BUFFERS; i++) {
      if (d.buf[i].ahb && d.buf[i].res_id == res_id) {
         present_adopted(i);
         return true;
      }
   }

   struct virgl_renderer_resource_info info;
   static unsigned rejected;
   if (virgl_renderer_resource_get_info((int)res_id, &info) != 0) {
      if (rejected++ < 5) fprintf(stderr, "hypocket-display: resource %u: no info\n", res_id);
      return false;
   }
   bool format_ok = false;
   switch (info.virgl_format) {
   case 1: case 2:      /* B8G8R8A8/X8_UNORM */
   case 67: case 134:   /* R8G8B8A8/X8_UNORM */
   case 68:             /* X8B8G8R8_UNORM */
   case 131: case 233:  /* B10G10R10A2/X2_UNORM: EGL picks it for an "8 bits or more" pbuffer */
   case 8: case 308:    /* R10G10B10A2/X2_UNORM */
      /* The copy shader samples, so any normalized colour format works. */
      format_ok = true;
      break;
   }
   if (!info.tex_id || info.depth > 1 || info.width != width || info.height != height || !format_ok) {
      if (rejected++ < 5)
         fprintf(stderr, "hypocket-display: resource %u rejected: tex %u, %ux%ux%u (wanted %ux%u), format %u, flags 0x%x\n",
                 res_id, info.tex_id, info.width, info.height, info.depth, width, height, info.virgl_format,
                 info.flags);
      return false;
   }

   if (!make_context()) {
      disable("cannot create the display context");
      return false;
   }
   EGLContext prev_ctx = eglGetCurrentContext();
   EGLSurface prev_draw = eglGetCurrentSurface(EGL_DRAW);
   EGLSurface prev_read = eglGetCurrentSurface(EGL_READ);

   /* Make the guest's rendering visible to our context. */
   EGLSyncKHR ready = eglCreateSyncKHR(d.dpy, EGL_SYNC_FENCE_KHR, NULL);
   glFlush();
   eglMakeCurrent(d.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, d.ctx);
   if (ready != EGL_NO_SYNC_KHR) {
      if (d.has_wait_sync)
         eglWaitSyncKHR(d.dpy, ready, 0);
      else
         eglClientWaitSyncKHR(d.dpy, ready, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR);
      eglDestroySyncKHR(d.dpy, ready);
   } else {
      glFinish();
   }

   if ((info.width != d.width || info.height != d.height) && !make_buffers(info.width, info.height)) {
      disable("cannot create the display buffers");
      goto restore;
   }
   read_releases();
   if (d.state < 0) goto restore;

   int free_index = -1;
   for (int i = 0; i < BUFFERS; i++) if (!d.buf[i].busy) { free_index = i; break; }
   if (free_index < 0) {
      /* The screen is behind: drop this frame instead of making the guest wait. */
      d.dropped++;
      goto restore;
   }
   struct buffer *b = &d.buf[free_index];

   glBindFramebuffer(GL_FRAMEBUFFER, b->fbo);
   glViewport(0, 0, (GLsizei)info.width, (GLsizei)info.height);
   glDisable(GL_BLEND);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_SCISSOR_TEST);
   /* Dark grey first: if the screen shows grey, the copy below is what fails. */
   glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);
   glUseProgram(d.program);
   glUniform2i(d.size_location, (GLint)info.width, (GLint)info.height);
   /* Our buffer's first row is the screen's top row. */
   glUniform1i(d.flip_location, y0_top ? 0 : 1);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, info.tex_id);
   glBindSampler(0, d.sampler);
   glBindVertexArray(d.vao);
   glDrawArrays(GL_TRIANGLES, 0, 3);
   check_gl("copy");
   if (d.frames == 0) {
      /* Diagnostics: what actually landed in the shared buffer. */
      unsigned char px[4] = { 0 };
      glReadPixels((GLint)info.width / 2, (GLint)info.height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
      fprintf(stderr, "hypocket-display: first frame %ux%u format %u tex %u, centre pixel %u %u %u %u\n",
              info.width, info.height, info.virgl_format, info.tex_id, px[0], px[1], px[2], px[3]);
      check_gl("read");
   }

   int fence_fd = -1;
   EGLSyncKHR done = d.has_native_fence ? eglCreateSyncKHR(d.dpy, EGL_SYNC_NATIVE_FENCE_ANDROID, NULL)
                                        : EGL_NO_SYNC_KHR;
   glFlush();
   if (done != EGL_NO_SYNC_KHR) {
      fence_fd = eglDupNativeFenceFDANDROID(d.dpy, done);
      eglDestroySyncKHR(d.dpy, done);
   }
   if (fence_fd < 0) glFinish();

   b->busy = true;
   if (!send_header(MSG_FRAME, (uint32_t)free_index, d.width, d.height, fence_fd)) disable("cannot send a frame");
   if (fence_fd >= 0) close(fence_fd);
   d.frames++;

restore:
   eglMakeCurrent(d.dpy, prev_draw, prev_read, prev_ctx);
   return true;
}

#else /* !__ANDROID__ */

bool hypocket_display_enabled(void) { return false; }
bool hypocket_display_skip_readback(void) { return false; }
bool hypocket_display_present(uint32_t res_id, uint32_t width, uint32_t height, bool y0_top)
{
   (void)res_id; (void)width; (void)height; (void)y0_top;
   return false;
}
void hypocket_display_adopt(uint32_t res_id) { (void)res_id; }
void hypocket_display_forget(uint32_t res_id) { (void)res_id; }

#endif
