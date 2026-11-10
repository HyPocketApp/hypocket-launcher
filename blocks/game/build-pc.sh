#!/bin/sh
# Builds HyPocket Blocks for PC with SDL2 (platform_sdl.c), into out/pc-linux
# and out/pc-windows, each with the sound files in media/ and SDL's license.
#
#   ./build-pc.sh linux     needs gcc and SDL2's development files
#                           (Debian/Ubuntu: libsdl2-dev)
#   ./build-pc.sh windows   needs MinGW-w64 (x86_64-w64-mingw32-gcc-posix);
#                           downloads SDL2's MinGW development files once
#   ./build-pc.sh           both
#
# The Linux program needs SDL2 installed (libsdl2-2.0-0); the Windows one
# carries SDL2.dll beside it. SDL2 is under the zlib license.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out="$here/out"
sdl_version=2.30.9
flags="-O2 -s -Wall -Wextra -Wno-unused-parameter -DBLOCKS_LIBRARY"
sources="$here/platform_sdl.c $here/blocks.c"

copy_media() {
    rm -rf "$1/media"
    mkdir -p "$1/media"
    (cd "$here/../assets/audio" && find . -name '*.ogg' -exec cp --parents {} "$1/media/" \;)
}

linux() {
    dir="$out/pc-linux"
    mkdir -p "$dir"
    # shellcheck disable=SC2086
    gcc -O2 -w -c -o "$out/lua-linux.o" "$here/third_party/lua_one.c"
    gcc -O2 -w -c -o "$out/wasm3-linux.o" "$here/third_party/wasm3_one.c"
    gcc $flags $(pkg-config --cflags sdl2) -o "$dir/blocks" $sources "$out/lua-linux.o" "$out/wasm3-linux.o" \
        $(pkg-config --libs sdl2) -lm -pthread
    copy_media "$dir"
    echo "Built $dir/blocks"
}

windows() {
    dir="$out/pc-windows"
    sdl="$out/SDL2-$sdl_version/x86_64-w64-mingw32"
    if [ ! -d "$sdl" ]; then
        curl -sSfL -o "$out/sdl2-mingw.tar.gz" \
            "https://github.com/libsdl-org/SDL/releases/download/release-$sdl_version/SDL2-devel-$sdl_version-mingw.tar.gz"
        tar xzf "$out/sdl2-mingw.tar.gz" -C "$out"
    fi
    mkdir -p "$dir"
    # shellcheck disable=SC2086
    x86_64-w64-mingw32-gcc-posix -O2 -w -c -o "$out/lua-windows.o" "$here/third_party/lua_one.c"
    x86_64-w64-mingw32-gcc-posix -O2 -w -c -o "$out/wasm3-windows.o" "$here/third_party/wasm3_one.c"
    x86_64-w64-mingw32-gcc-posix $flags -I"$sdl/include/SDL2" -o "$dir/blocks.exe" $sources \
        "$out/lua-windows.o" "$out/wasm3-windows.o" \
        -L"$sdl/lib" -lmingw32 -lSDL2main -lSDL2 -lm -static-libgcc -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic -mwindows
    cp "$sdl/bin/SDL2.dll" "$dir/"
    cp "$out/SDL2-$sdl_version/LICENSE.txt" "$dir/SDL2-LICENSE.txt"
    copy_media "$dir"
    echo "Built $dir/blocks.exe"
}

mkdir -p "$out"
case "${1:-all}" in
    linux) linux ;;
    windows) windows ;;
    all) linux; windows ;;
    *) echo "usage: $0 [linux|windows]" >&2; exit 2 ;;
esac
