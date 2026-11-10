/* HyPocket Blocks: runs the game through the native platform interface
 * (platform.h) with no window, for testing on a Linux machine without a
 * screen: an EGL pbuffer stands in for the window. With HEADLESS_GLES=1 the
 * context is OpenGL ES 3.0, as on the phone; else desktop OpenGL 3.3 core.
 * HEADLESS_SHOT=<file.ppm> keeps the last frame.
 *
 * Build: see build.sh (headless). Usage: blocks-headless <w> <h> <fps> <frames> */
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../third_party/GL/glcorearb.h"
#include "../platform.h"

static EGLDisplay display;
static EGLSurface surface;
static int width, height;

static void* get_proc(const char* name) { return (void*)eglGetProcAddress(name); }

static void present(void) {
    const char* shot = getenv("HEADLESS_SHOT");
    if (shot) {
        PFNGLREADPIXELSPROC read = (PFNGLREADPIXELSPROC)eglGetProcAddress("glReadPixels");
        unsigned char* px = malloc((size_t)width * height * 4);
        read(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, px);
        FILE* f = fopen(shot, "wb");
        if (f) {
            fprintf(f, "P6\n%d %d\n255\n", width, height);
            for (int y = height - 1; y >= 0; y--)
                for (int x = 0; x < width; x++) fwrite(px + ((size_t)y * width + x) * 4, 1, 3, f);
            fclose(f);
        }
        free(px);
    }
    eglSwapBuffers(display, surface);
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <width> <height> <fps> <frames>\n", argv[0]);
        return 2;
    }
    width = atoi(argv[1]);
    height = atoi(argv[2]);
    int gles = getenv("HEADLESS_GLES") != NULL;
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(display, NULL, NULL) || !eglBindAPI(gles ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) return 1;
    const EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                              gles ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                              EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    if (!eglChooseConfig(display, attribs, &config, 1, &count) || !count) return 1;
    const EGLint size[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, size);
    const EGLint es_ctx[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    const EGLint gl_ctx[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
                             EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, gles ? es_ctx : gl_ctx);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
        fprintf(stderr, "headless: no context (0x%x)\n", eglGetError());
        return 1;
    }
    static const BlocksPlatform platform = {.get_proc = get_proc, .present = present};
    BlocksPlatform* p = malloc(sizeof *p);
    memcpy(p, &platform, sizeof *p);
    p->gles = gles;
    blocks_platform = p;
    return blocks_main(argc, argv);
}
