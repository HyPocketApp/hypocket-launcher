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
- The story lives in `docs/LORE.md`. The author decides it: only put story
  that is under "Definido" into the game, and ask before inventing more.
- After every change to the game, build the APK and send it to the user as a
  download (`timberheart-NN.apk`, counting up), even when they did not ask.
- The script of the scenes is in `src/story.c` (speaker, English, Portuguese).
  It is the author's text: keep it word for word; only split a line into two
  pages when it does not fit the box (the game warns at start-up).
- Sprites go in `assets/` as PNG at their real pixel size; the build embeds
  them (`cmake/embed_assets.cmake`, `asset_find()`). Keep the shape-drawn
  fallback for anything without an image. The author sends art upscaled
  (e.g. 2048x2048 for 32x32): shrink it back by sampling cell centres.
- Grass: `assets/tiles/grass_N.png`, 32x32, each covering 2x2 tiles, drawn by
  `tools/make_grass.py` from the author's palette (no bright corners, so no grid).
- Trees (16x24, hang 8 px over the tile above), the stump and the dirt path
  (fill, ragged grass edges, inside corners) come from `tools/make_tiles.py`.
  Do not put chests, fires or doors right above a tree: its top covers them.
- Pines, birches, rocks, water (frames, shores, rounded corners), bridges,
  bushes, flowers and the house come from `tools/make_props.py`; monsters
  (slime, beetle, wolf, two frames each) from `tools/make_monsters.py`,
  written as letter grids. Trees grow in groves; each area has its own mix
  of oak, pine and birch (`tree_texture()` in main.c).
- Maps: `#` is deep forest drawn as trees. Shape areas with irregular forest
  edges, winding paths and ponds; avoid straight walls and square blocks.
