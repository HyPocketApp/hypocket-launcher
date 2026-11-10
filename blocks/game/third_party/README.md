# Third-party code

Code from other projects, kept as it came. Each entry names where it came
from and its license, which must be compatible with GPL-3.0.

| File | What | Source | License |
|---|---|---|---|
| `stb_vorbis.c` | Ogg Vorbis decoder, v1.22 | https://github.com/nothings/stb (by Sean Barrett) | MIT or public domain (Unlicense), the reader's choice; text at the end of the file. Both are compatible with GPL-3.0. |

`blocks.c` includes `stb_vorbis.c` directly, so `build.sh` needs nothing
more.
