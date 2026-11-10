# Third-party code

Code from other projects, kept as it came. Each entry names where it came
from and its license, which must be compatible with GPL-3.0.

| File | What | Source | License |
|---|---|---|---|
| `stb_vorbis.c` | Ogg Vorbis decoder, v1.22 | https://github.com/nothings/stb (by Sean Barrett) | MIT or public domain (Unlicense), the reader's choice; text at the end of the file. Both are compatible with GPL-3.0. |
| `stb_image.h` | PNG decoder for the mods' textures, v2.30 | https://github.com/nothings/stb (by Sean Barrett) | MIT or public domain (Unlicense), the reader's choice; text at the end of the file. |
| `font8x8/` | 8x8 bitmap font (Basic Latin and Latin-1) for the game's own screens | https://github.com/dhepper/font8x8 (by Daniel Hepper, from IBM's public domain VGA fonts) | Public domain; see its README. |
| `lua/`, `lua_one.c` | Lua 5.4.7, for mods' scripts (only the base, table, string, math, utf8 and coroutine libraries are built) | https://github.com/lua/lua, tag v5.4.7 (Lua.org, PUC-Rio) | MIT; text at the end of `lua/lua.h`. |
| `wasm3/`, `wasm3_one.c` | wasm3 0.5.0, a WebAssembly interpreter for mods' compiled scripts (without its WASI and libc parts) | https://github.com/wasm3/wasm3, tag v0.5.0 (Steven Massey, Volodymyr Shymanskyy) | MIT; `wasm3/LICENSE`. |
| `GL/glcorearb.h`, `KHR/khrplatform.h` | OpenGL type and name declarations, used for OpenGL and OpenGL ES 3 alike | https://github.com/KhronosGroup/OpenGL-Registry and EGL-Registry (The Khronos Group) | MIT; text at the top of each file. |

`blocks.c` includes these directly, so `build.sh` needs nothing more.
