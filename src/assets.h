// Images and music from the assets/ folder, built into the program (see
// cmake/embed_assets.cmake). Each one is found by its path inside assets/
// without ".png" or ".ogg", for example "tiles/grass_1" or "music/flute".
#ifndef TIMBERHEART_ASSETS_H
#define TIMBERHEART_ASSETS_H

typedef struct {
    const char *name;
    const unsigned char *data;
    unsigned size;
} EmbeddedAsset;

// NULL when there is no such file: the game then draws its own shapes
// (or plays no music).
const EmbeddedAsset *asset_find(const char *name);

#endif
