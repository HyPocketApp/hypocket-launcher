// Audio bridge. A guest program (through Box64) cannot reach Android's audio
// APIs, so it writes 16-bit stereo samples into a ring in a shared file, and
// this side plays them with AAudio from the stream's data callback.
//
// File layout (little endian), must match engine/guest-tests/blocks/blocks.c:
//   0  uint32 magic "HPBA"         4  uint32 version (1)
//   8  uint32 sample rate          12 uint32 channels (2)
//   16 uint32 capacity in frames   20 uint32 reserved
//   24 uint64 write position, in frames (written by the guest)
//   32 uint64 read position, in frames (written here)
//   40 uint32 underruns (written here)
//   64 int16 samples, capacity * channels
//
// Positions only grow; the sample index is position % capacity. The guest
// keeps a little audio ahead of the read position; when it falls behind,
// this side plays silence and counts an underrun.

#include "audio_output.h"

#include <aaudio/AAudio.h>
#include <android/log.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

namespace hypocket {

namespace {

constexpr uint32_t kMagic = 0x41425048;  // "HPBA"
constexpr uint32_t kRate = 48000;        // AAudio converts if the device differs
constexpr uint32_t kChannels = 2;
constexpr uint32_t kCapacity = 8192;  // frames, about 170 ms
constexpr size_t kHeader = 64;

struct Ring {
    uint32_t magic, version, rate, channels, capacity, reserved;
    uint64_t write_pos, read_pos;
    uint32_t underruns;
};

}  // namespace

struct AudioState {
    int fd = -1;
    void* map = nullptr;
    size_t size = 0;
    Ring* ring = nullptr;
    int16_t* samples = nullptr;

    std::mutex mutex;  // guards stream
    AAudioStream* stream = nullptr;
    std::atomic<bool> closing{false};
    std::atomic<uint64_t> played{0};
    // Only the data callback uses these: whether the guest is still writing.
    uint64_t last_write = 0;
    int64_t idle_frames = 0;
    std::atomic<int> restarts{0};
    int32_t burst = 0, buffer = 0, device_rate = 0;
    bool low_latency = false;
    std::weak_ptr<AudioState> self;  // keeps the state alive for a reopen
};

namespace {

aaudio_data_callback_result_t OnData(AAudioStream* /* stream */, void* user, void* audio, int32_t frames) {
    auto* s = static_cast<AudioState*>(user);
    auto* out = static_cast<int16_t*>(audio);
    Ring* r = s->ring;
    uint64_t write = __atomic_load_n(&r->write_pos, __ATOMIC_ACQUIRE);
    uint64_t read = __atomic_load_n(&r->read_pos, __ATOMIC_RELAXED);
    // A guest that skipped ahead or restarted: resynchronise.
    if (write < read || write - read > kCapacity) read = write;
    auto available = static_cast<int32_t>(std::min<uint64_t>(write - read, static_cast<uint64_t>(frames)));
    for (int32_t i = 0; i < available; i++) {
        uint32_t index = static_cast<uint32_t>((read + static_cast<uint64_t>(i)) % kCapacity);
        out[i * 2] = s->samples[index * 2];
        out[i * 2 + 1] = s->samples[index * 2 + 1];
    }
    // A guest that stopped writing (it exited, or has not started) is not
    // late: only count underruns while it writes.
    if (write != s->last_write) {
        s->last_write = write;
        s->idle_frames = 0;
    } else {
        s->idle_frames += frames;
    }
    if (available < frames) {
        memset(out + available * 2, 0, static_cast<size_t>(frames - available) * kChannels * sizeof(int16_t));
        if (write > 0 && s->idle_frames < kRate / 4) __atomic_fetch_add(&r->underruns, 1, __ATOMIC_RELAXED);
    }
    __atomic_store_n(&r->read_pos, read + static_cast<uint64_t>(available), __ATOMIC_RELEASE);
    s->played += static_cast<uint64_t>(available);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

bool OpenStream(AudioState& s);

void OnError(AAudioStream* /* stream */, void* user, aaudio_result_t error) {
    auto* state = static_cast<AudioState*>(user);
    std::shared_ptr<AudioState> s = state->self.lock();
    if (error != AAUDIO_ERROR_DISCONNECTED || !s || s->closing) return;
    // The output changed (headphones, Bluetooth): reopen on another thread,
    // as AAudio asks. The ring and its positions stay the same.
    std::thread([s] {
        std::lock_guard<std::mutex> lock(s->mutex);
        if (s->closing) return;
        if (s->stream) {
            AAudioStream_close(s->stream);
            s->stream = nullptr;
        }
        if (OpenStream(*s)) s->restarts++;
    }).detach();
}

// Called with s.mutex held, or before the state is shared.
bool OpenStream(AudioState& s) {
    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) return false;
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, kChannels);
    AAudioStreamBuilder_setSampleRate(builder, kRate);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_GAME);
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_SONIFICATION);
    AAudioStreamBuilder_setDataCallback(builder, OnData, &s);
    AAudioStreamBuilder_setErrorCallback(builder, OnError, &s);
    AAudioStream* stream = nullptr;
    aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);
    if (result != AAUDIO_OK || stream == nullptr) return false;
    // Two bursts of buffering: low latency without crackling.
    s.burst = AAudioStream_getFramesPerBurst(stream);
    AAudioStream_setBufferSizeInFrames(stream, s.burst * 2);
    s.buffer = AAudioStream_getBufferSizeInFrames(stream);
    s.device_rate = AAudioStream_getSampleRate(stream);
    s.low_latency = AAudioStream_getPerformanceMode(stream) == AAUDIO_PERFORMANCE_MODE_LOW_LATENCY;
    if (AAudioStream_requestStart(stream) != AAUDIO_OK) {
        AAudioStream_close(stream);
        return false;
    }
    s.stream = stream;
    return true;
}

}  // namespace

std::shared_ptr<AudioState>* AudioOutputCreate(const char* path) {
    auto state = std::make_shared<AudioState>();
    AudioState& s = *state;
    s.self = state;
    s.size = kHeader + static_cast<size_t>(kCapacity) * kChannels * sizeof(int16_t);
    s.fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (s.fd < 0 || ftruncate(s.fd, static_cast<off_t>(s.size)) != 0) {
        if (s.fd >= 0) close(s.fd);
        return nullptr;
    }
    s.map = mmap(nullptr, s.size, PROT_READ | PROT_WRITE, MAP_SHARED, s.fd, 0);
    if (s.map == MAP_FAILED) {
        close(s.fd);
        return nullptr;
    }
    s.ring = static_cast<Ring*>(s.map);
    s.samples = reinterpret_cast<int16_t*>(static_cast<char*>(s.map) + kHeader);
    s.ring->version = 1;
    s.ring->rate = kRate;
    s.ring->channels = kChannels;
    s.ring->capacity = kCapacity;
    if (!OpenStream(s)) {
        munmap(s.map, s.size);
        close(s.fd);
        __android_log_print(ANDROID_LOG_WARN, "HyPocketAudio", "no audio stream");
        return nullptr;
    }
    // Published last: the guest only uses a ring with the magic set.
    __atomic_store_n(&s.ring->magic, kMagic, __ATOMIC_RELEASE);
    return new std::shared_ptr<AudioState>(state);
}

std::string AudioOutputInfo(std::shared_ptr<AudioState>* handle) {
    AudioState& s = **handle;
    std::lock_guard<std::mutex> lock(s.mutex);
    char text[256];
    snprintf(text, sizeof(text),
             "AAudio %u Hz (device %d Hz), %s, burst %d frames, buffer %d frames, %.1f s played, "
             "%u underruns, %d restarts",
             kRate, s.device_rate, s.low_latency ? "low latency" : "normal latency", s.burst, s.buffer,
             static_cast<double>(s.played.load()) / kRate, __atomic_load_n(&s.ring->underruns, __ATOMIC_RELAXED),
             s.restarts.load());
    return text;
}

void AudioOutputDestroy(std::shared_ptr<AudioState>* handle) {
    std::shared_ptr<AudioState> state = *handle;
    delete handle;
    AudioState& s = *state;
    s.closing = true;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.stream) {
            AAudioStream_requestStop(s.stream);
            AAudioStream_close(s.stream);
            s.stream = nullptr;
        }
    }
    munmap(s.map, s.size);
    close(s.fd);
}

}  // namespace hypocket
