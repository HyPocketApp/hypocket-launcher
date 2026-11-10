#!/bin/sh
# Builds HyPocket Blocks, the x86-64 Linux test game. Run on an x86-64 Linux
# machine with gcc and the EGL/GL development files (Debian/Ubuntu: libegl-dev
# libgl-dev). The output is committed so other contributors do not need an
# x86-64 toolchain.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out="$here/../src/main/assets/guest-tests/blocks-x86_64"
gcc -O2 -s -Wall -Wextra -Wno-unused-parameter -o "$out" "$here/blocks.c" -lEGL -lm -pthread
echo "Built $out"

# The same game through the native platform interface, without a window,
# for tests on this machine (tools/headless.c). Not committed.
if [ "${1:-}" = "headless" ]; then
    mkdir -p "$here/out"
    gcc -O2 -Wall -Wextra -Wno-unused-parameter -DBLOCKS_LIBRARY -o "$here/out/blocks-headless" \
        "$here/tools/headless.c" "$here/blocks.c" -lEGL -lm -pthread
    echo "Built $here/out/blocks-headless"
fi
