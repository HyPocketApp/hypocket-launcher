// The story scenes: who speaks on each page and what they say, in English
// and Brazilian Portuguese. Plain data, no raylib, so the rules (game.c)
// can count the pages and the tests can check them.
#ifndef TIMBERHEART_STORY_H
#define TIMBERHEART_STORY_H

#include <stdbool.h>

#include "game.h"

typedef enum {
    SPEAKER_NONE,  // narration
    SPEAKER_HERO,
    SPEAKER_UNKNOWN,  // "???"
    SPEAKER_WOMAN,
    SPEAKER_CREATURE_1,
    SPEAKER_CREATURE_2,
    SPEAKER_CREATURE_3,
    SPEAKER_COUNT,
} Speaker;

typedef struct {
    Speaker who;
    const char *en, *pt;
} StoryPage;

int story_page_count(Scene scene);
const StoryPage *story_page(Scene scene, int page);  // NULL when out of range
const char *story_speaker_name(Speaker who, bool portuguese);

#endif
