#!/bin/sh
# Builds the x86-64 Linux graphics performance probe. Run on an x86-64 Linux
# machine with gcc and the EGL/GL development files (Debian/Ubuntu:
# libegl-dev libgl-dev). The output is committed so other contributors do not
# need an x86-64 toolchain.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out="$here/../../src/main/assets/guest-tests/gl-perf-probe-x86_64"
gcc -O2 -s -Wall -o "$out" "$here/gl_perf_probe.c" -lEGL
echo "Built $out"
