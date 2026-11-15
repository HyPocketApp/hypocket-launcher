// Saved games: one file per game, as many as the player makes, and a small
// settings file. Plain C, no raylib, so the tests can use it too.
#ifndef TIMBERHEART_SAVE_H
#define TIMBERHEART_SAVE_H

#include <stdbool.h>

#include "game.h"

typedef struct {
    int slot;           // the game's number, from 1
    int level;
    int area;
    long long saved_at; // seconds since 1970
    int order;          // counts up with every save, to sort them
} SaveInfo;

// The folder the files go in; it is created when something is written.
void save_set_dir(const char *dir);
const char *save_dir(void);

// Writes the game to its slot. A game without one (save_slot 0) gets the
// next free number first.
bool save_write(Game *g);
// Reads a save into `g`. False when it is missing, damaged or from a version
// of the game that kept other things.
bool save_read(int slot, Game *g);
// All the saves, newest first. Free the list with free().
SaveInfo *save_list(int *count);

bool settings_write(bool portuguese, bool sound_on);
bool settings_read(bool *portuguese, bool *sound_on);

#endif
