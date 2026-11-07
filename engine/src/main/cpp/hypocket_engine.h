// Native engine API. This header must not depend on JNI so the engine can be
// built and tested without Android. The CPU and graphics translation layers
// will be added behind this API.
#pragma once

#include <cstdint>

namespace hypocket {

// Version of the native engine.
const char* EngineVersion();

// ABI the engine was compiled for, for example "arm64-v8a".
const char* EngineAbi();

struct WorkloadResult {
    uint64_t checksum;
    int64_t elapsed_ms;
};

// Runs the probe workload natively, to compare with the translated run.
WorkloadResult RunNativeWorkload();

}  // namespace hypocket
