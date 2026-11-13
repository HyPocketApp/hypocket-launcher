# Notes for AI coding tools

- Timberheart is an original turn-based 2D RPG (top-down map, menu battles) in C
  with raylib, for PC and Android. The hero is a lumberjack with an axe. It is
  not related to any other project or game; do not copy code, names or art
  from other games.
- Keep `src/game.c`, `src/game.h` and `src/levels.c` free of raylib and of any
  platform code, so `tests/` can run anywhere. Drawing, input and sound go in
  `src/main.c`.
- Player-facing text goes in `src/text.c`, in English and Brazilian Portuguese.
- Code, comments and commits in English (Conventional Commits). The README is in
  Brazilian Portuguese.
- Target low-end phones: draw at 320x180, draw only what is visible, no big assets.
- After changing the rules, the maps or the balance, run the tests (see README).
- Battle effects are applied when a round is resolved; the screen catches up
  through the messages (`shown_hp`, `*_after` fields). Keep that split.
