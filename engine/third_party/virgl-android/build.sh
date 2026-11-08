#!/bin/sh
# Builds the VirGL render server for Android (bionic, arm64): libepoxy,
# libvirglrenderer and virgl_test_server. The server receives GL commands
# from Mesa's virgl driver in the guest and runs them on the phone's own
# OpenGL ES driver.
#
# Usage: build.sh <work dir> <output dir> <ndk dir> <min sdk>
# Needs: git, meson >= 1.4 (a venv is created if needed), ninja, pkg-config.
set -eu

EPOXY_TAG=1.5.10
EPOXY_COMMIT=c84bc9459357a40e46e2fec0408d04fbdde2c973
VIRGL_TAG=1.3.0
VIRGL_COMMIT=ca50e008863837e094747a69974dde3ae148aeaa

here=$(cd "$(dirname "$0")" && pwd)
work=$1
out=$2
ndk=$3
api=$4
toolchain="$ndk/toolchains/llvm/prebuilt/$(uname -s | tr '[:upper:]' '[:lower:]')-x86_64"
prefix="$work/prefix"

mkdir -p "$work" "$out"
cd "$work"
. "$here/../meson-env.sh"

fetch() { # name url tag commit
    if [ ! -d "$1" ]; then git clone -q --depth 1 -b "$3" "$2" "$1"; fi
    test "$(git -C "$1" rev-parse HEAD)" = "$4" || { echo "$1: unexpected commit" >&2; exit 1; }
}
fetch libepoxy https://github.com/anholt/libepoxy.git "$EPOXY_TAG" "$EPOXY_COMMIT"
fetch virglrenderer https://gitlab.freedesktop.org/virgl/virglrenderer.git "$VIRGL_TAG" "$VIRGL_COMMIT"
git -C virglrenderer checkout -q -- .
git -C virglrenderer apply "$here/android-egl.patch"
# Direct display: show frames on the app's screen without a copy (display/).
git -C virglrenderer apply "$here/android-display.patch"
cp "$here/display/hypocket_display.c" "$here/display/hypocket_display.h" virglrenderer/vtest/

cat > android.cross <<CROSS
[binaries]
c = '$toolchain/bin/aarch64-linux-android$api-clang'
cpp = '$toolchain/bin/aarch64-linux-android$api-clang++'
ar = '$toolchain/bin/llvm-ar'
strip = '$toolchain/bin/llvm-strip'
pkg-config = 'pkg-config'

[properties]
pkg_config_libdir = '$prefix/lib/pkgconfig'

[built-in options]
c_args = ['-I$prefix/include', '-I$here/shim']
c_link_args = ['-L$prefix/lib', '-llog', '-lnativewindow']

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
CROSS

rm -rf libepoxy/build virglrenderer/build
meson setup libepoxy/build libepoxy --cross-file android.cross --prefix="$prefix" --libdir=lib \
    -Dglx=no -Dx11=false -Degl=yes -Dtests=false -Ddocs=false
ninja -C libepoxy/build install
meson setup virglrenderer/build virglrenderer --cross-file android.cross --prefix="$prefix" --libdir=lib \
    -Dplatforms=egl -Dvenus=false -Ddrm-renderers=[] -Dtests=false -Dvideo=false
ninja -C virglrenderer/build

cp "$prefix/lib/libepoxy.so" "$out/libepoxy.so"
cp virglrenderer/build/src/libvirglrenderer.so "$out/libvirglrenderer.so"
# Android only lets apps run files named lib*.so from their native library dir.
cp virglrenderer/build/vtest/virgl_test_server "$out/libvirgl_server.so"
echo "VirGL server for Android written to $out"
