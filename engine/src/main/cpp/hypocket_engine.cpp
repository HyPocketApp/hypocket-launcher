#include "hypocket_engine.h"

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

}  // namespace hypocket
