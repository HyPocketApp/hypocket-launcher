# HyPocket Blocks, native on Android: the same blocks.c as the emulated
# build, with platform_android.c around it (see blocks/game/platform.h).
cmake_minimum_required(VERSION 3.22)
project(blocks C)

set(GAME ${CMAKE_CURRENT_SOURCE_DIR}/../../../game)

add_library(blocks SHARED ${GAME}/blocks.c ${GAME}/platform_android.c
    ${GAME}/third_party/lua_one.c ${GAME}/third_party/wasm3_one.c)
# Lua and wasm3 are not our code: no warnings for them.
set_source_files_properties(${GAME}/third_party/lua_one.c ${GAME}/third_party/wasm3_one.c PROPERTIES COMPILE_OPTIONS -w)
target_compile_definitions(blocks PRIVATE BLOCKS_LIBRARY)
target_compile_options(blocks PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(blocks EGL aaudio android log m dl)
