# Notes for AI coding tools

- Mossling is an original top-down 2D game in C with raylib, for PC and Android.
  It is not related to any other project or game; do not copy code, names or
  art from other games.
- Keep `src/game.c`, `src/game.h` and `src/levels.c` free of raylib and of any
  platform code, so `tests/` can run anywhere. Drawing, input and sound go in
  `src/main.c`.
- Player-facing text goes in `src/text.c`, in English and Brazilian Portuguese.
- Code, comments and commits in English (Conventional Commits). The README is in
  Brazilian Portuguese.
- Target low-end phones: draw at 320x180, draw only what is visible, no big assets.
- After changing the rules or the maps, run the tests (see README).
