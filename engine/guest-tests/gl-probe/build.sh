#!/bin/sh
# Builds the x86-64 Linux graphics probe. Run on an x86-64 Linux machine with
# gcc and the EGL/GLES development files (Debian/Ubuntu: libegl-dev
# libgles-dev). The output is committed so other contributors do not need
# an x86-64 toolchain.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out="$here/../../src/main/assets/guest-tests/gl-probe-x86_64"
gcc -O2 -s -o "$out" "$here/gl_probe.c" -lEGL -lGLESv2
echo "Built $out"
