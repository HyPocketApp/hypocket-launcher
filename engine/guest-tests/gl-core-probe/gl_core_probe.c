/* x86-64 Linux test program for desktop OpenGL. The game needs an OpenGL 4.1
 * core profile context, so this asks EGL for desktop GL core contexts from
 * 4.6 down to 3.2, reports the highest one that works, then draws a triangle
 * with it off screen and checks the colors. GL functions are loaded with
 * eglGetProcAddress, so only libEGL is linked. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <stdio.h>
#include <string.h>

#define SIZE 256
#define REQUIRED_MAJOR 4
#define REQUIRED_MINOR 1

#define GL_FUNCTIONS(X) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLGETINTEGERVPROC, glGetIntegerv) \
    X(PFNGLGETERRORPROC, glGetError) \
    X(PFNGLVIEWPORTPROC, glViewport) \
    X(PFNGLCLEARCOLORPROC, glClearColor) \
    X(PFNGLCLEARPROC, glClear) \
    X(PFNGLFINISHPROC, glFinish) \
    X(PFNGLREADPIXELSPROC, glReadPixels) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)

#define DECLARE(type, name) static type p_##name;
GL_FUNCTIONS(DECLARE)

static int load_gl(void) {
#define LOAD(type, name) \
    p_##name = (type)eglGetProcAddress(#name); \
    if (!p_##name) { printf("gl-core-probe: missing %s\n", #name); return 0; }
    GL_FUNCTIONS(LOAD)
    return 1;
}

static GLuint compile(GLenum type, const char* source) {
    GLuint shader = p_glCreateShader(type);
    p_glShaderSource(shader, 1, &source, NULL);
    p_glCompileShader(shader);
    GLint ok = 0;
    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        p_glGetShaderInfoLog(shader, sizeof log, NULL, log);
        printf("gl-core-probe: shader error: %s\n", log);
    }
    return shader;
}

int main(void) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint egl_major = 0, egl_minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &egl_major, &egl_minor)) {
        printf("gl-core-probe: eglInitialize failed (0x%x)\n", eglGetError());
        return 1;
    }
    printf("gl-core-probe: EGL %d.%d, client APIs: %s\n", egl_major, egl_minor,
           eglQueryString(display, EGL_CLIENT_APIS));
    if (!eglBindAPI(EGL_OPENGL_API)) {
        printf("gl-core-probe: desktop OpenGL API not available\n");
        return 1;
    }

    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE,
    };
    EGLConfig config;
    EGLint count = 0;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &count) || count == 0) {
        printf("gl-core-probe: no desktop OpenGL config\n");
        return 1;
    }
    const EGLint surface_attribs[] = {EGL_WIDTH, SIZE, EGL_HEIGHT, SIZE, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attribs);

    static const int versions[][2] = {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2}};
    EGLContext context = EGL_NO_CONTEXT;
    int major = 0, minor = 0;
    for (size_t i = 0; i < sizeof versions / sizeof versions[0]; i++) {
        const EGLint context_attribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, versions[i][0],
            EGL_CONTEXT_MINOR_VERSION, versions[i][1],
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            EGL_NONE,
        };
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
        printf("gl-core-probe: core %d.%d: %s\n", versions[i][0], versions[i][1],
               context != EGL_NO_CONTEXT ? "yes" : "no");
        if (context != EGL_NO_CONTEXT) {
            major = versions[i][0];
            minor = versions[i][1];
            break;
        }
    }
    if (context == EGL_NO_CONTEXT) {
        printf("gl-core-probe: no desktop OpenGL core context\n");
        return 1;
    }
    if (!eglMakeCurrent(display, surface, surface, context) || !load_gl()) {
        printf("gl-core-probe: could not use the context\n");
        return 1;
    }

    printf("gl-core-probe: highest core %d.%d\n", major, minor);
    printf("gl-core-probe: GL_RENDERER %s\n", p_glGetString(GL_RENDERER));
    printf("gl-core-probe: GL_VERSION %s\n", p_glGetString(GL_VERSION));
    printf("gl-core-probe: GLSL %s\n", p_glGetString(GL_SHADING_LANGUAGE_VERSION));

    /* Draw into a framebuffer object, as core profile games do. */
    GLuint fbo, color;
    p_glGenFramebuffers(1, &fbo);
    p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    p_glGenRenderbuffers(1, &color);
    p_glBindRenderbuffer(GL_RENDERBUFFER, color);
    p_glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, SIZE, SIZE);
    p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    if (p_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("gl-core-probe: framebuffer incomplete\n");
        return 1;
    }

    GLuint program = p_glCreateProgram();
    p_glAttachShader(program, compile(GL_VERTEX_SHADER,
        "#version 150 core\n"
        "in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n"));
    p_glAttachShader(program, compile(GL_FRAGMENT_SHADER,
        "#version 150 core\n"
        "out vec4 color;\n"
        "void main() { color = vec4(1.0, 0.0, 0.0, 1.0); }\n"));
    p_glLinkProgram(program);
    GLint linked = 0;
    p_glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        printf("gl-core-probe: link failed\n");
        return 1;
    }
    p_glUseProgram(program);

    const GLfloat vertices[] = {-0.8f, -0.8f, 0.8f, -0.8f, 0.0f, 0.8f};
    GLuint vao, vbo;
    p_glGenVertexArrays(1, &vao);
    p_glBindVertexArray(vao);
    p_glGenBuffers(1, &vbo);
    p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    p_glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
    p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    p_glEnableVertexAttribArray(0);

    p_glViewport(0, 0, SIZE, SIZE);
    p_glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    p_glClear(GL_COLOR_BUFFER_BIT);
    p_glDrawArrays(GL_TRIANGLES, 0, 3);
    p_glFinish();

    unsigned char centre[4] = {0}, corner[4] = {0};
    p_glReadPixels(SIZE / 2, SIZE / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, centre);
    p_glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, corner);
    printf("gl-core-probe: centre %d,%d,%d corner %d,%d,%d (GL error 0x%x)\n",
           centre[0], centre[1], centre[2], corner[0], corner[1], corner[2], p_glGetError());

    int colors_ok = centre[0] > 200 && centre[1] < 50 && centre[2] < 50 &&
                    corner[0] < 50 && corner[1] < 50 && corner[2] > 200;
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(display);
    if (!colors_ok) {
        printf("gl-core-probe: wrong colors\n");
        return 2;
    }
    if (major < REQUIRED_MAJOR || (major == REQUIRED_MAJOR && minor < REQUIRED_MINOR)) {
        printf("gl-core-probe: drew correctly, but below the required %d.%d\n", REQUIRED_MAJOR, REQUIRED_MINOR);
        return 3;
    }
    printf("gl-core-probe: ok\n");
    return 0;
}
