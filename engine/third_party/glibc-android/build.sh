#!/bin/sh
# Builds glibc for arm64 with HyPocket's Android patch (android-seccomp.patch).
#
# Android's app seccomp filter kills a process that calls set_robust_list,
# rseq, clone3, faccessat2 or fchmodat2. Stock glibc calls some of these at
# startup, so it cannot run inside an Android app. The patch makes glibc
# behave as on a kernel without them.
#
# Usage: build.sh <work dir> <output dir>
# Needs: aarch64-linux-gnu-gcc, make, bison, gawk, python3.
set -eu

VERSION=2.39
SHA256=f77bd47cf8170c57365ae7bf86696c118adb3b120d3259c64c502d3dc1e2d926
here=$(cd "$(dirname "$0")" && pwd)
work=$1
out=$2

mkdir -p "$work" "$out"
cd "$work"
tarball=glibc-$VERSION.tar.xz
if [ ! -f "$tarball" ]; then
    curl -fsSLo "$tarball.tmp" "https://ftp.gnu.org/gnu/glibc/$tarball"
    mv "$tarball.tmp" "$tarball"
fi
echo "$SHA256  $tarball" | sha256sum -c -

rm -rf src build
mkdir src build
tar -xf "$tarball" -C src --strip-components=1
(cd src && patch -p1 < "$here/android-seccomp.patch")

cd build
../src/configure --host=aarch64-linux-gnu --build="$(../src/scripts/config.guess)" \
    --prefix=/usr --disable-werror --disable-profile --without-selinux \
    CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++
make -j"$(nproc)"

cp elf/ld.so "$out/ld-linux-aarch64.so.1"
cp libc.so "$out/libc.so.6"
cp math/libm.so "$out/libm.so.6"
cp resolv/libresolv.so "$out/libresolv.so.2"
cp dlfcn/libdl.so "$out/libdl.so.2"
cp nptl/libpthread.so "$out/libpthread.so.0"
cp rt/librt.so "$out/librt.so.1"
cp login/libutil.so "$out/libutil.so.1"
echo "glibc $VERSION for Android written to $out"
