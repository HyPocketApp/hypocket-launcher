/* Lua 5.4.7 (third_party/lua, MIT) built as one file. Only the libraries
 * the mods get are here: no io, os, package or debug (see scripts.c). */
#define LUA_CORE
#if !defined(_WIN32) && !defined(__ANDROID__)
#define LUA_USE_POSIX
#endif
#include "lua/lapi.c"
#include "lua/lcode.c"
#include "lua/lctype.c"
#include "lua/ldebug.c"
#include "lua/ldo.c"
#include "lua/ldump.c"
#include "lua/lfunc.c"
#include "lua/lgc.c"
#include "lua/llex.c"
#include "lua/lmem.c"
#include "lua/lobject.c"
#include "lua/lopcodes.c"
#include "lua/lparser.c"
#include "lua/lstate.c"
#include "lua/lstring.c"
#include "lua/ltable.c"
#include "lua/ltm.c"
#include "lua/lundump.c"
#include "lua/lvm.c"
#include "lua/lzio.c"
#include "lua/lauxlib.c"
#include "lua/lbaselib.c"
#include "lua/lcorolib.c"
#include "lua/lmathlib.c"
#include "lua/lstrlib.c"
#include "lua/ltablib.c"
#include "lua/lutf8lib.c"
