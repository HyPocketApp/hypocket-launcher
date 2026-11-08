// Direct display: shows frames that the VirGL server renders into
// AHardwareBuffers, by handing them to the system compositor through
// ASurfaceControl. No pixel goes through the CPU. The protocol is described
// in engine/third_party/virgl-android/display/hypocket_display.c.
//
// ASurfaceControl needs API 29; on older phones create() fails and the app
// keeps the copying display path.

#include "direct_display.h"

#include <android/hardware_buffer.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/surface_control.h>
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
#include <thread>

namespace hypocket {

constexpr int kBuffers = 3;

struct State {
    ANativeWindow* window = nullptr;
    ASurfaceControl* surface = nullptr;
    int listen_fd = -1;
    int conn_fd = -1;
    std::atomic<bool> stop{false};
    std::atomic<int> shown{0};
    std::atomic<int> view_width{0}, view_height{0};
    std::mutex mutex;  // guards buffers, sizes, on_screen and writes to conn_fd
    AHardwareBuffer* buffers[kBuffers] = {};
    uint32_t width = 0, height = 0;
    int on_screen = -1;
    std::thread thread;
};

namespace {

constexpr uint32_t kMsgBuffer = 1, kMsgFrame = 2, kMsgRelease = 3;

void Log(const char* msg) { __android_log_print(ANDROID_LOG_INFO, "HyPocketDisplay", "%s", msg); }

struct Pending {
    std::shared_ptr<State> state;
    int previous;
};

bool SendRelease(State& s, int id) {
    uint32_t msg[4] = {kMsgRelease, static_cast<uint32_t>(id), 0, 0};
    return s.conn_fd >= 0 && send(s.conn_fd, msg, sizeof(msg), MSG_NOSIGNAL) == sizeof(msg);
}

// Called by the system once a frame is on screen: the frame before it is no
// longer shown, so the server may draw into its buffer again.
void OnComplete(void* context, ASurfaceTransactionStats* stats) {
    std::unique_ptr<Pending> pending(static_cast<Pending*>(context));
    State& s = *pending->state;
    s.shown++;
    if (pending->previous < 0) return;
    if (__builtin_available(android 29, *)) {
        int fence = ASurfaceTransactionStats_getPreviousReleaseFenceFd(stats, s.surface);
        if (fence >= 0) {
            pollfd p{fence, POLLIN, 0};
            poll(&p, 1, 200);
            close(fence);
        }
    }
    std::lock_guard<std::mutex> lock(s.mutex);
    SendRelease(s, pending->previous);
}

void Present(const std::shared_ptr<State>& state, int id, int fence) {
    State& s = *state;
    if (__builtin_available(android 29, *)) {
        std::lock_guard<std::mutex> lock(s.mutex);
        AHardwareBuffer* buffer = (id >= 0 && id < kBuffers) ? s.buffers[id] : nullptr;
        if (!buffer) {
            if (fence >= 0) close(fence);
            return;
        }
        // Fit the frame in the view, keeping its aspect ratio.
        int vw = s.view_width.load(), vh = s.view_height.load();
        float scale = std::min(static_cast<float>(vw) / s.width, static_cast<float>(vh) / s.height);
        int w = static_cast<int>(s.width * scale), h = static_cast<int>(s.height * scale);
        ARect source{0, 0, static_cast<int32_t>(s.width), static_cast<int32_t>(s.height)};
        ARect destination{(vw - w) / 2, (vh - h) / 2, (vw - w) / 2 + w, (vh - h) / 2 + h};

        ASurfaceTransaction* t = ASurfaceTransaction_create();
        ASurfaceTransaction_setBuffer(t, s.surface, buffer, fence);  // takes the fence
        ASurfaceTransaction_setGeometry(t, s.surface, source, destination, 0);
        ASurfaceTransaction_setVisibility(t, s.surface, ASURFACE_TRANSACTION_VISIBILITY_SHOW);
        ASurfaceTransaction_setOnComplete(t, new Pending{state, s.on_screen}, OnComplete);
        s.on_screen = id;
        ASurfaceTransaction_apply(t);
        ASurfaceTransaction_delete(t);
    } else if (fence >= 0) {
        close(fence);
    }
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
    // Wait for the server, checking now and then whether we should stop.
    while (!s.stop) {
        pollfd p{s.listen_fd, POLLIN, 0};
        if (poll(&p, 1, 200) > 0) break;
    }
    if (s.stop) return;
    int fd = accept4(s.listen_fd, nullptr, nullptr, SOCK_CLOEXEC);
    if (fd < 0) return;
    // Only our own processes (same Linux user) may send us frames.
    ucred cred{};
    socklen_t len = sizeof(cred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0 || cred.uid != getuid()) {
        Log("rejected a connection from another app");
        close(fd);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.conn_fd = fd;
    }
    Log("server connected");

    uint32_t msg[4];
    int passed;
    while (!s.stop && ReadHeader(fd, msg, &passed)) {
        if (msg[0] == kMsgBuffer && msg[1] < kBuffers) {
            if (passed >= 0) close(passed);
            AHardwareBuffer* buffer = nullptr;
            if (AHardwareBuffer_recvHandleFromUnixSocket(fd, &buffer) != 0) break;
            std::lock_guard<std::mutex> lock(s.mutex);
            if (s.buffers[msg[1]]) AHardwareBuffer_release(s.buffers[msg[1]]);
            s.buffers[msg[1]] = buffer;
            s.width = msg[2];
            s.height = msg[3];
            // Buffers of the old size are gone: nothing is on screen from this set.
            s.on_screen = -1;
        } else if (msg[0] == kMsgFrame) {
            Present(state, static_cast<int>(msg[1]), passed);
        } else if (passed >= 0) {
            close(passed);
        }
    }
    Log("server disconnected");
}

}  // namespace

std::shared_ptr<State>* DirectDisplayCreate(ANativeWindow* window, char* socket_name, size_t name_size) {
    if (__builtin_available(android 29, *)) {
        auto state = std::make_shared<State>();
        state->window = window;
        state->view_width = ANativeWindow_getWidth(window);
        state->view_height = ANativeWindow_getHeight(window);
        state->surface = ASurfaceControl_createFromWindow(window, "hypocket-direct-display");
        if (!state->surface) return nullptr;

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
            ASurfaceControl_release(state->surface);
            return nullptr;
        }
        state->thread = std::thread(Serve, state);
        return new std::shared_ptr<State>(state);
    }
    return nullptr;
}

void DirectDisplayResize(std::shared_ptr<State>* handle, int width, int height) {
    (*handle)->view_width = width;
    (*handle)->view_height = height;
}

int DirectDisplayShownFrames(std::shared_ptr<State>* handle) { return (*handle)->shown.load(); }

void DirectDisplayDestroy(std::shared_ptr<State>* handle) {
    std::shared_ptr<State> state = *handle;
    delete handle;
    State& s = *state;
    s.stop = true;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.conn_fd >= 0) shutdown(s.conn_fd, SHUT_RDWR);
    }
    if (s.thread.joinable()) s.thread.join();
    if (__builtin_available(android 29, *)) {
        ASurfaceTransaction* t = ASurfaceTransaction_create();
        ASurfaceTransaction_reparent(t, s.surface, nullptr);
        ASurfaceTransaction_apply(t);
        ASurfaceTransaction_delete(t);
        ASurfaceControl_release(s.surface);
    }
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.conn_fd >= 0) close(s.conn_fd);
    s.conn_fd = -1;
    close(s.listen_fd);
    for (auto& b : s.buffers) {
        if (b) AHardwareBuffer_release(b);
        b = nullptr;
    }
    ANativeWindow_release(s.window);
    // Callbacks still pending hold their own reference to the state.
}

}  // namespace hypocket
