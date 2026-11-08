// Direct display: shows frames that the VirGL server renders into
// AHardwareBuffers. The buffers are imported as EGL images and drawn onto the
// view's own window with OpenGL ES, so no pixel goes through the CPU. The
// protocol is described in
// engine/third_party/virgl-android/display/hypocket_display.c.
//
// One thread owns the socket and the EGL context: it receives buffers and
// frames, draws each frame and swaps (which also paces it to the screen's
// refresh rate).

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include "direct_display.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <android/native_window.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace hypocket {

constexpr int kBuffers = 3;

struct Buffer {
    AHardwareBuffer* ahb = nullptr;
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    GLuint texture = 0;
    EGLSyncKHR read_done = EGL_NO_SYNC_KHR;  // the last draw that read this buffer
};

struct State {
    ANativeWindow* window = nullptr;
    int listen_fd = -1;
    int conn_fd = -1;
    std::atomic<bool> stop{false};
    std::atomic<int> shown{0};
    std::thread thread;

    // Owned by the thread.
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    GLuint program = 0, vao = 0;
    GLint scale_location = -1;
    Buffer buffers[kBuffers];
    uint32_t width = 0, height = 0;
    int previous = -1;

    std::mutex info_mutex;
    std::string info;  // for the test report
    int received_buffers = 0;
};

namespace {

constexpr uint32_t kMsgBuffer = 1, kMsgFrame = 2, kMsgRelease = 3;

void Note(State& s, const char* text) {
    __android_log_print(ANDROID_LOG_INFO, "HyPocketDisplay", "%s", text);
    std::lock_guard<std::mutex> lock(s.info_mutex);
    if (s.info.size() < 2000) s.info += std::string(text) + "\n";
}

const char* kVertex =
    "#version 300 es\n"
    "uniform vec2 scale;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
    "    uv = vec2(p.x, 1.0 - p.y);\n"  // buffer row 0 is the top of the image
    "    gl_Position = vec4((p * 2.0 - 1.0) * scale, 0.0, 1.0);\n"
    "}\n";
const char* kFragment =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D frame;\n"
    "in vec2 uv;\n"
    "out vec4 color;\n"
    "void main() { color = vec4(texture(frame, uv).rgb, 1.0); }\n";

GLuint Compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool SetUpEgl(State& s) {
    s.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s.display == EGL_NO_DISPLAY || !eglInitialize(s.display, nullptr, nullptr)) return false;
    const EGLint config_attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                     EGL_RED_SIZE,        8,
                                     EGL_GREEN_SIZE,      8,
                                     EGL_BLUE_SIZE,       8,
                                     EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    if (!eglChooseConfig(s.display, config_attribs, &config, 1, &count) || count == 0) return false;
    const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    s.context = eglCreateContext(s.display, config, EGL_NO_CONTEXT, context_attribs);
    if (s.context == EGL_NO_CONTEXT) return false;
    s.surface = eglCreateWindowSurface(s.display, config, s.window, nullptr);
    if (s.surface == EGL_NO_SURFACE) return false;
    if (!eglMakeCurrent(s.display, s.surface, s.surface, s.context)) return false;

    GLuint vs = Compile(GL_VERTEX_SHADER, kVertex), fs = Compile(GL_FRAGMENT_SHADER, kFragment);
    if (!vs || !fs) return false;
    s.program = glCreateProgram();
    glAttachShader(s.program, vs);
    glAttachShader(s.program, fs);
    glLinkProgram(s.program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(s.program, GL_LINK_STATUS, &linked);
    if (!linked) return false;
    glUseProgram(s.program);
    glUniform1i(glGetUniformLocation(s.program, "frame"), 0);
    s.scale_location = glGetUniformLocation(s.program, "scale");
    glGenVertexArrays(1, &s.vao);
    // Show black until the first frame arrives.
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(s.display, s.surface);
    return true;
}

void FreeBuffer(State& s, Buffer& b) {
    if (b.read_done != EGL_NO_SYNC_KHR) eglDestroySyncKHR(s.display, b.read_done);
    if (b.texture) glDeleteTextures(1, &b.texture);
    if (b.image != EGL_NO_IMAGE_KHR) eglDestroyImageKHR(s.display, b.image);
    if (b.ahb) AHardwareBuffer_release(b.ahb);
    b = Buffer{};
}

void TearDownEgl(State& s) {
    if (s.display == EGL_NO_DISPLAY) return;
    for (auto& b : s.buffers) FreeBuffer(s, b);
    if (s.program) glDeleteProgram(s.program);
    if (s.vao) glDeleteVertexArrays(1, &s.vao);
    eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s.surface != EGL_NO_SURFACE) eglDestroySurface(s.display, s.surface);
    if (s.context != EGL_NO_CONTEXT) eglDestroyContext(s.display, s.context);
    eglTerminate(s.display);
    s.display = EGL_NO_DISPLAY;
}

bool SendRelease(State& s, int id) {
    uint32_t msg[4] = {kMsgRelease, static_cast<uint32_t>(id), 0, 0};
    return send(s.conn_fd, msg, sizeof(msg), MSG_NOSIGNAL) == sizeof(msg);
}

bool ImportBuffer(State& s, Buffer& b) {
    EGLClientBuffer client = eglGetNativeClientBufferANDROID(b.ahb);
    const EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    b.image = eglCreateImageKHR(s.display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, client, attribs);
    if (b.image == EGL_NO_IMAGE_KHR) return false;
    glGenTextures(1, &b.texture);
    glBindTexture(GL_TEXTURE_2D, b.texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(b.image));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return true;
}

void DrawFrame(State& s, int id, int fence) {
    if (id < 0 || id >= kBuffers || !s.buffers[id].texture) {
        if (fence >= 0) close(fence);
        return;
    }
    // Let the GPU wait for the server's copy instead of blocking here.
    if (fence >= 0) {
        const EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fence, EGL_NONE};
        EGLSyncKHR sync = eglCreateSyncKHR(s.display, EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
        if (sync != EGL_NO_SYNC_KHR) {
            eglWaitSyncKHR(s.display, sync, 0);  // takes ownership of the fd
            eglDestroySyncKHR(s.display, sync);
        } else {
            pollfd p{fence, POLLIN, 0};
            poll(&p, 1, 100);
            close(fence);
        }
    }

    EGLint vw = 0, vh = 0;
    eglQuerySurface(s.display, s.surface, EGL_WIDTH, &vw);
    eglQuerySurface(s.display, s.surface, EGL_HEIGHT, &vh);
    // Fit the frame in the view, keeping its aspect ratio.
    float fx = static_cast<float>(vw) / s.width, fy = static_cast<float>(vh) / s.height;
    float fit = std::min(fx, fy);
    glViewport(0, 0, vw, vh);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(s.program);
    glUniform2f(s.scale_location, fit / fx, fit / fy);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s.buffers[id].texture);
    glBindVertexArray(s.vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    Buffer& b = s.buffers[id];
    if (b.read_done != EGL_NO_SYNC_KHR) eglDestroySyncKHR(s.display, b.read_done);
    b.read_done = eglCreateSyncKHR(s.display, EGL_SYNC_FENCE_KHR, nullptr);
    if (s.shown == 0) {
        GLenum error = glGetError();
        char text[96];
        snprintf(text, sizeof(text), "first frame drawn: view %dx%d, frame %ux%u, GL error 0x%x", vw, vh, s.width,
                 s.height, error);
        Note(s, text);
    }
    eglSwapBuffers(s.display, s.surface);
    s.shown++;

    // The previous frame is no longer drawn: once the GPU is done reading it,
    // the server may use its buffer again.
    if (s.previous >= 0 && s.previous != id) {
        Buffer& old = s.buffers[s.previous];
        if (old.read_done != EGL_NO_SYNC_KHR) {
            eglClientWaitSyncKHR(s.display, old.read_done, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 100000000);
            eglDestroySyncKHR(s.display, old.read_done);
            old.read_done = EGL_NO_SYNC_KHR;
        }
        SendRelease(s, s.previous);
    }
    s.previous = id;
}

// Reads one 16-byte header, and the file descriptor sent with it, if any.
bool ReadHeader(int fd, uint32_t msg[4], int* passed_fd) {
    *passed_fd = -1;
    size_t got = 0;
    while (got < 16) {
        iovec iov{reinterpret_cast<char*>(msg) + got, 16 - got};
        char control[CMSG_SPACE(sizeof(int))];
        msghdr mh{};
        mh.msg_iov = &iov;
        mh.msg_iovlen = 1;
        mh.msg_control = control;
        mh.msg_controllen = sizeof(control);
        ssize_t n = recvmsg(fd, &mh, MSG_CMSG_CLOEXEC);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        for (cmsghdr* c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) {
            if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) memcpy(passed_fd, CMSG_DATA(c), sizeof(int));
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

void Serve(std::shared_ptr<State> state) {
    State& s = *state;
    if (!SetUpEgl(s)) {
        Note(s, "EGL setup failed");
        TearDownEgl(s);
        return;
    }
    Note(s, "EGL ready");
    // Wait for the server, checking now and then whether we should stop.
    while (!s.stop) {
        pollfd p{s.listen_fd, POLLIN, 0};
        if (poll(&p, 1, 200) > 0) break;
    }
    int fd = s.stop ? -1 : accept4(s.listen_fd, nullptr, nullptr, SOCK_CLOEXEC);
    if (fd >= 0) {
        // Only our own processes (same Linux user) may send us frames.
        ucred cred{};
        socklen_t len = sizeof(cred);
        if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0 || cred.uid != getuid()) {
            Note(s, "rejected a connection from another app");
            close(fd);
            fd = -1;
        }
    }
    if (fd >= 0) {
        s.conn_fd = fd;
        Note(s, "server connected");
        uint32_t msg[4];
        int passed;
        while (!s.stop && ReadHeader(fd, msg, &passed)) {
            if (msg[0] == kMsgBuffer && msg[1] < kBuffers) {
                if (passed >= 0) close(passed);
                Buffer& b = s.buffers[msg[1]];
                FreeBuffer(s, b);
                if (AHardwareBuffer_recvHandleFromUnixSocket(fd, &b.ahb) != 0 || !ImportBuffer(s, b)) {
                    Note(s, "could not import a buffer");
                    break;
                }
                s.width = msg[2];
                s.height = msg[3];
                s.previous = -1;  // a new set of buffers: none of them is on screen
                s.received_buffers++;
            } else if (msg[0] == kMsgFrame) {
                DrawFrame(s, static_cast<int>(msg[1]), passed);
            } else if (passed >= 0) {
                close(passed);
            }
        }
        Note(s, "server disconnected");
    }
    TearDownEgl(s);
}

}  // namespace

std::shared_ptr<State>* DirectDisplayCreate(ANativeWindow* window, char* socket_name, size_t name_size) {
    auto state = std::make_shared<State>();
    state->window = window;
    static std::atomic<int> counter{0};
    snprintf(socket_name, name_size, "hypocket-display-%d-%d", getpid(), counter++);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    size_t name_len = strlen(socket_name);
    memcpy(addr.sun_path + 1, socket_name, name_len);  // abstract namespace
    auto addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + name_len);
    state->listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (state->listen_fd < 0 || bind(state->listen_fd, reinterpret_cast<sockaddr*>(&addr), addr_len) != 0 ||
        listen(state->listen_fd, 1) != 0) {
        if (state->listen_fd >= 0) close(state->listen_fd);
        return nullptr;
    }
    state->thread = std::thread(Serve, state);
    return new std::shared_ptr<State>(state);
}

void DirectDisplayResize(std::shared_ptr<State>* /* handle */, int /* width */, int /* height */) {
    // The EGL window surface follows the view's size by itself.
}

int DirectDisplayShownFrames(std::shared_ptr<State>* handle) { return (*handle)->shown.load(); }

std::string DirectDisplayInfo(std::shared_ptr<State>* handle) {
    State& s = **handle;
    std::lock_guard<std::mutex> lock(s.info_mutex);
    return s.info + "buffers received: " + std::to_string(s.received_buffers) +
           ", frames drawn: " + std::to_string(s.shown.load());
}

void DirectDisplayDestroy(std::shared_ptr<State>* handle) {
    std::shared_ptr<State> state = *handle;
    delete handle;
    State& s = *state;
    s.stop = true;
    if (s.conn_fd >= 0) shutdown(s.conn_fd, SHUT_RDWR);
    if (s.thread.joinable()) s.thread.join();
    if (s.conn_fd >= 0) close(s.conn_fd);
    close(s.listen_fd);
    ANativeWindow_release(s.window);
}

}  // namespace hypocket
