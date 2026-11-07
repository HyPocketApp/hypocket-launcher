#include "hypocket_engine.h"

#include <chrono>

#include "probe_workload.h"

namespace hypocket {

const char* EngineVersion() {
    return "0.1.0";
}

const char* EngineAbi() {
#if defined(__aarch64__)
    return "arm64-v8a";
#elif defined(__x86_64__)
    return "x86_64";
#else
    return "unsupported";
#endif
}

WorkloadResult RunNativeWorkload() {
    const auto start = std::chrono::steady_clock::now();
    const uint64_t checksum = probe_workload();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return {checksum, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()};
}

}  // namespace hypocket
