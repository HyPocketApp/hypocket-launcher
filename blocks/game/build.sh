#!/bin/sh
# Builds HyPocket Blocks, the x86-64 Linux test game. Run on an x86-64 Linux
# machine with gcc and the EGL/GL development files (Debian/Ubuntu: libegl-dev
# libgl-dev). The output is committed so other contributors do not need an
# x86-64 toolchain.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
# The tables the game's own screens take from the Android side (generated.h).
python3 "$here/tools/gen_tables.py"
mkdir -p "$here/out"
out="$here/../src/main/assets/guest-tests/blocks-x86_64"
# Lua and wasm3 (third_party) are separate files, built without warnings: not our code.
gcc -O2 -w -c -o "$here/out/lua.o" "$here/third_party/lua_one.c"
gcc -O2 -w -c -o "$here/out/wasm3.o" "$here/third_party/wasm3_one.c"
gcc -O2 -s -Wall -Wextra -Wno-unused-parameter -o "$out" "$here/blocks.c" "$here/out/lua.o" "$here/out/wasm3.o" -lEGL -lm -pthread
echo "Built $out"

# The same game through the native platform interface, without a window,
# for tests on this machine (tools/headless.c). Not committed.
if [ "${1:-}" = "headless" ]; then
    mkdir -p "$here/out"
    gcc -O2 -Wall -Wextra -Wno-unused-parameter -DBLOCKS_LIBRARY -o "$here/out/blocks-headless" \
        "$here/tools/headless.c" "$here/blocks.c" "$here/out/lua.o" "$here/out/wasm3.o" -lEGL -lm -pthread
    echo "Built $here/out/blocks-headless"
fi
