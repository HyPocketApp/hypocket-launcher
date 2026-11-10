# Third-party code

Code from other projects, kept as it came. Each entry names where it came
from and its license, which must be compatible with GPL-3.0.

| File | What | Source | License |
|---|---|---|---|
| `stb_vorbis.c` | Ogg Vorbis decoder, v1.22 | https://github.com/nothings/stb (by Sean Barrett) | MIT or public domain (Unlicense), the reader's choice; text at the end of the file. Both are compatible with GPL-3.0. |
| `stb_image.h` | PNG decoder for the mods' textures, v2.30 | https://github.com/nothings/stb (by Sean Barrett) | MIT or public domain (Unlicense), the reader's choice; text at the end of the file. |
| `font8x8/` | 8x8 bitmap font (Basic Latin and Latin-1) for the game's own screens | https://github.com/dhepper/font8x8 (by Daniel Hepper, from IBM's public domain VGA fonts) | Public domain; see its README. |
| `GL/glcorearb.h`, `KHR/khrplatform.h` | OpenGL type and name declarations, used for OpenGL and OpenGL ES 3 alike | https://github.com/KhronosGroup/OpenGL-Registry and EGL-Registry (The Khronos Group) | MIT; text at the top of each file. |

`blocks.c` includes these directly, so `build.sh` needs nothing more.
