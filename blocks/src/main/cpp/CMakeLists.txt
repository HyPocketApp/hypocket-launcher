# HyPocket Blocks, native on Android: the same blocks.c as the emulated
# build, with platform_android.c around it (see blocks/game/platform.h).
cmake_minimum_required(VERSION 3.22)
project(blocks C)

set(GAME ${CMAKE_CURRENT_SOURCE_DIR}/../../../game)

add_library(blocks SHARED ${GAME}/blocks.c ${GAME}/platform_android.c)
target_compile_definitions(blocks PRIVATE BLOCKS_LIBRARY)
target_compile_options(blocks PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(blocks EGL aaudio android log m dl)
