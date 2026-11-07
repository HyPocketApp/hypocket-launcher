// Native engine API. This header must not depend on JNI so the engine can be
// built and tested without Android. The CPU and graphics translation layers
// will be added behind this API.
#pragma once

namespace hypocket {

// Version of the native engine.
const char* EngineVersion();

// ABI the engine was compiled for, for example "arm64-v8a".
const char* EngineAbi();

}  // namespace hypocket
