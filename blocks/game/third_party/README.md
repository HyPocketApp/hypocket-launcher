# Third-party code

Code from other projects, kept as it came. Each entry names where it came
from and its license, which must be compatible with GPL-3.0.

| File | What | Source | License |
|---|---|---|---|
| `stb_vorbis.c` | Ogg Vorbis decoder, v1.22 | https://github.com/nothings/stb (by Sean Barrett) | MIT or public domain (Unlicense), the reader's choice; text at the end of the file. Both are compatible with GPL-3.0. |
| `GL/glcorearb.h`, `KHR/khrplatform.h` | OpenGL type and name declarations, used for OpenGL and OpenGL ES 3 alike | https://github.com/KhronosGroup/OpenGL-Registry and EGL-Registry (The Khronos Group) | MIT; text at the top of each file. |

`blocks.c` includes these directly, so `build.sh` needs nothing more.
