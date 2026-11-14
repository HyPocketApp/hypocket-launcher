// Images from the assets/ folder, built into the program (see
// cmake/embed_assets.cmake). Each one is found by its path inside assets/
// without ".png", for example "tiles/grass_1".
#ifndef TIMBERHEART_ASSETS_H
#define TIMBERHEART_ASSETS_H

typedef struct {
    const char *name;
    const unsigned char *data;
    unsigned size;
} EmbeddedAsset;

// NULL when there is no such image: the game then draws its own shapes.
const EmbeddedAsset *asset_find(const char *name);

#endif
