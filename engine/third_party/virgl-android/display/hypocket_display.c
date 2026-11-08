/*
 * HyPocket direct display for the VirGL server.
 * Copyright (C) 2026 HyPocket Launcher contributors. GPL-3.0-or-later.
 *
 * The app listens on an abstract Unix socket (HYPOCKET_DISPLAY_SOCKET). This
 * server allocates a few AHardwareBuffers of the frame's size, sends them to
 * the app once, and for every frame copies the guest's texture into a free
 * buffer on the GPU and tells the app which buffer to show, with a native
 * fence. The app hands the buffer to the system compositor, so the frame
 * never goes through the CPU.
 *
 * Messages (little endian, 16 bytes): uint32 type, id, width, height.
 *   server -> app  1 BUFFER  followed by the AHardwareBuffer handle
 *   server -> app  2 FRAME   with the fence fd (if any) attached
 *   app -> server  3 RELEASE the buffer is no longer on screen
 */
#include "hypocket_display.h"

#ifdef __ANDROID__

#include <android/hardware_buffer.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "virglrenderer.h"

/* Exported by our patch to src/vrend/vrend_winsys_egl.c. */
bool hypocket_egl_get(EGLDisplay *display, EGLConfig *config, EGLContext *context);

#define BUFFERS 3
enum { MSG_BUFFER = 1, MSG_FRAME = 2, MSG_RELEASE = 3 };

struct buffer {
   AHardwareBuffer *ahb;
   EGLImageKHR image;
   GLuint tex, fbo;
   bool busy;
};

static struct {
   int state; /* 0 not tried, 1 on, -1 off */
   int sock;
   bool skip_readback;
   bool has_wait_sync, has_native_fence;
   EGLDisplay dpy;
   EGLContext ctx;
   GLuint read_fbo;
   uint32_t width, height;
   struct buffer buf[BUFFERS];
   unsigned frames, dropped;
} d = { .sock = -1 };

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

static bool send_header(uint32_t type, uint32_t id, int fd)
{
   uint32_t msg[4] = { type, id, d.width, d.height };
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
   const EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   d.ctx = eglCreateContext(d.dpy, config, share, attribs);
   if (d.ctx == EGL_NO_CONTEXT) {
      disable("cannot create the display context");
      return false;
   }
   EGLContext prev = eglGetCurrentContext();
   EGLSurface prev_draw = eglGetCurrentSurface(EGL_DRAW), prev_read = eglGetCurrentSurface(EGL_READ);
   eglMakeCurrent(d.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, d.ctx);
   bool has_image = epoxy_has_gl_extension("GL_OES_EGL_image");
   eglMakeCurrent(d.dpy, prev_draw, prev_read, prev);
   if (!has_image) {
      disable("the driver lacks GL_OES_EGL_image");
      return false;
   }
   d.state = 1;
   log_msg("on");
   return true;
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
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
      if (!send_header(MSG_BUFFER, i, -1) || AHardwareBuffer_sendHandleToUnixSocket(b->ahb, d.sock) != 0)
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
      if (msg[0] == MSG_RELEASE && msg[1] < BUFFERS) d.buf[msg[1]].busy = false;
   }
}

bool hypocket_display_present(uint32_t res_id, uint32_t width, uint32_t height)
{
   if (!init()) return false;

   struct virgl_renderer_resource_info info;
   if (virgl_renderer_resource_get_info((int)res_id, &info) != 0 || !info.tex_id || info.depth > 1 ||
       info.width != width || info.height != height)
      return false;
   switch (info.virgl_format) {
   case 1: case 2: case 67: case 134: break; /* B8G8R8A8/X8, R8G8B8A8/X8 UNORM */
   default: return false;
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

   if (!d.read_fbo) glGenFramebuffers(1, &d.read_fbo);
   glBindFramebuffer(GL_READ_FRAMEBUFFER, d.read_fbo);
   glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, info.tex_id, 0);
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER, b->fbo);
   /* GL's first row is the bottom one; the screen's is the top one. */
   glBlitFramebuffer(0, 0, (GLint)info.width, (GLint)info.height,
                     0, (GLint)info.height, (GLint)info.width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
   glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

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
   if (!send_header(MSG_FRAME, (uint32_t)free_index, fence_fd)) disable("cannot send a frame");
   if (fence_fd >= 0) close(fence_fd);
   d.frames++;

restore:
   eglMakeCurrent(d.dpy, prev_draw, prev_read, prev_ctx);
   return true;
}

#else /* !__ANDROID__ */

bool hypocket_display_enabled(void) { return false; }
bool hypocket_display_skip_readback(void) { return false; }
bool hypocket_display_present(uint32_t res_id, uint32_t width, uint32_t height)
{
   (void)res_id; (void)width; (void)height;
   return false;
}

#endif
