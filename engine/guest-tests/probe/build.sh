#!/bin/sh
# Builds the x86-64 Linux probe used by the CPU translator test.
# Run on an x86-64 Linux machine with gcc. The output is committed so other
# contributors do not need an x86-64 toolchain.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out="$here/../../src/main/assets/guest-tests/probe-x86_64"
gcc -O2 -fno-asynchronous-unwind-tables -s \
    -o "$out" "$here/probe_main.c" "$here/probe_workload.c"
echo "Built $out"
