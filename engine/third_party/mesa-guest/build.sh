#!/bin/sh
# Builds Mesa for arm64 glibc (the guest side) with only the virgl driver:
# libEGL, libGLESv2 and libgallium. Box64 forwards the game's EGL/GL calls to
# these native libraries, and the virgl driver sends them to the VirGL server
# running on Android (see ../virgl-android).
#
# Usage: build.sh <work dir> <output dir>
# Needs: aarch64-linux-gnu-gcc/g++, meson >= 1.4 (a venv is created if
# needed), ninja, python3-mako, python3-yaml, flex, bison, pkg-config, and the
# arm64 development packages libdrm-dev, libexpat1-dev and zlib1g-dev
# (Debian/Ubuntu multiarch: `dpkg --add-architecture arm64`).
set -eu

VERSION=26.2.6
SHA256=b9436b3e1a8be76fbd4823dc884f90596ab7c6ac5e1867a305e66ffccfb31a92
multiarch=/usr/lib/aarch64-linux-gnu
here=$(cd "$(dirname "$0")" && pwd)
work=$1
out=$2

mkdir -p "$work" "$out"
cd "$work"
. "$here/../meson-env.sh"

tarball=mesa-$VERSION.tar.xz
if [ ! -f "$tarball" ]; then
    curl -fsSLo "$tarball.tmp" "https://archive.mesa3d.org/$tarball"
    mv "$tarball.tmp" "$tarball"
fi
echo "$SHA256  $tarball" | sha256sum -c -
rm -rf src build
mkdir src
tar -xf "$tarball" -C src --strip-components=1
# HyPocket: eglSwapBuffers on a pbuffer presents it through the VirGL server.
# The work dir can be inside another git checkout (engine/build is inside this
# repository): without the ceiling, git apply finds that repository, takes
# the patch's paths as relative to its top and skips them all silently.
(cd src && GIT_CEILING_DIRECTORIES="$work" git apply "$here/hypocket-present.patch")
grep -q HYPOCKET src/src/gallium/winsys/virgl/vtest/virgl_vtest_winsys.c || {
    echo "hypocket-present.patch was not applied" >&2; exit 1; }

cat > aarch64.cross <<CROSS
[binaries]
c = 'aarch64-linux-gnu-gcc'
cpp = 'aarch64-linux-gnu-g++'
ar = 'aarch64-linux-gnu-ar'
strip = 'aarch64-linux-gnu-strip'
pkg-config = 'pkg-config'

[properties]
pkg_config_libdir = '$multiarch/pkgconfig:/usr/share/pkgconfig'

[host_machine]
system = 'linux'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
CROSS

meson setup build src --cross-file aarch64.cross --buildtype=release -Dprefix=/usr \
    -Dgallium-drivers=virgl -Dvulkan-drivers= -Dplatforms= -Degl=enabled -Dgles1=disabled \
    -Dgles2=enabled -Dopengl=true -Dglx=disabled -Dgbm=disabled -Dglvnd=disabled \
    -Dllvm=disabled -Dvalgrind=disabled -Dlibunwind=disabled -Dlmsensors=disabled \
    -Dzstd=disabled -Dtools=
ninja -C build

cp -L build/src/egl/libEGL.so.1 "$out/"
cp -L build/src/mesa/glapi/es2api/libGLESv2.so.2 "$out/"
cp -L build/src/gallium/targets/dri/libgallium-$VERSION.so "$out/"
for lib in libdrm.so.2 libexpat.so.1 libz.so.1; do
    cp -L "$multiarch/$lib" "$out/"
done
echo "Mesa $VERSION (virgl) for arm64 glibc written to $out"
