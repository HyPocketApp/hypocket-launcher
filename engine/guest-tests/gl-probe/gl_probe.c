/* x86-64 Linux test program for the graphics path. It creates an off-screen
 * OpenGL ES 2 context with EGL, draws a triangle, reads pixels back and
 * checks their colors. It needs no window system, so it only tests that GL
 * calls reach the phone's GPU and come back correct. */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SIZE 256
#define FRAMES 120

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static GLuint compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof log, NULL, log);
        printf("gl-probe: shader error: %s\n", log);
    }
    return shader;
}

static int fail(const char* what) {
    printf("gl-probe: failed at %s (EGL error 0x%x, GL error 0x%x)\n", what, eglGetError(), glGetError());
    return 1;
}

int main(void) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor)) return fail("eglInitialize");
    printf("gl-probe: EGL %d.%d, vendor %s\n", major, minor, eglQueryString(display, EGL_VENDOR));

    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE,
    };
    EGLConfig config;
    EGLint count = 0;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &count) || count == 0) return fail("eglChooseConfig");

    const EGLint surface_attribs[] = {EGL_WIDTH, SIZE, EGL_HEIGHT, SIZE, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attribs);
    if (surface == EGL_NO_SURFACE) return fail("eglCreatePbufferSurface");

    if (!eglBindAPI(EGL_OPENGL_ES_API)) return fail("eglBindAPI");
    const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) return fail("eglCreateContext");
    if (!eglMakeCurrent(display, surface, surface, context)) return fail("eglMakeCurrent");

    printf("gl-probe: GL_VENDOR %s\n", glGetString(GL_VENDOR));
    printf("gl-probe: GL_RENDERER %s\n", glGetString(GL_RENDERER));
    printf("gl-probe: GL_VERSION %s\n", glGetString(GL_VERSION));

    GLuint program = glCreateProgram();
    glAttachShader(program, compile(GL_VERTEX_SHADER,
        "attribute vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n"));
    glAttachShader(program, compile(GL_FRAGMENT_SHADER,
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n"));
    glBindAttribLocation(program, 0, "pos");
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) return fail("glLinkProgram");
    glUseProgram(program);

    /* A triangle covering the centre of the image but not the corners. */
    const GLfloat vertices[] = {-0.8f, -0.8f, 0.8f, -0.8f, 0.0f, 0.8f};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(0);
    glViewport(0, 0, SIZE, SIZE);

    double start = now_ms();
    for (int i = 0; i < FRAMES; i++) {
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glFinish();
    double elapsed = now_ms() - start;

    unsigned char centre[4] = {0}, corner[4] = {0};
    glReadPixels(SIZE / 2, SIZE / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centre);
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, corner);
    printf("gl-probe: centre %d,%d,%d corner %d,%d,%d\n",
           centre[0], centre[1], centre[2], corner[0], corner[1], corner[2]);
    printf("gl-probe: %d frames in %.0f ms\n", FRAMES, elapsed);

    int centre_red = centre[0] > 200 && centre[1] < 50 && centre[2] < 50;
    int corner_blue = corner[0] < 50 && corner[1] < 50 && corner[2] > 200;
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(display);
    if (!centre_red || !corner_blue) {
        printf("gl-probe: wrong colors\n");
        return 2;
    }
    printf("gl-probe: ok\n");
    return 0;
}
