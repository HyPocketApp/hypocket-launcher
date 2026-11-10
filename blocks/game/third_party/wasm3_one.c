/* wasm3 0.5.0 (third_party/wasm3, MIT) built as one file: the interpreter
 * only, without its WASI or libc imports (see scripts_wasm.c). */
/* m3_core.c defines the error constants, but only if wasm3.h was not read before it. */
#define M3_IMPLEMENT_ERROR_STRINGS
#include "wasm3/m3_bind.c"
#include "wasm3/m3_code.c"
#include "wasm3/m3_compile.c"
#include "wasm3/m3_core.c"
#include "wasm3/m3_emit.c"
#include "wasm3/m3_env.c"
#include "wasm3/m3_exec.c"
#include "wasm3/m3_function.c"
#include "wasm3/m3_info.c"
#include "wasm3/m3_module.c"
#include "wasm3/m3_optimize.c"
#include "wasm3/m3_parse.c"
