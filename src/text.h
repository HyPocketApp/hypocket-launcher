// Words shown to the player, in English and Brazilian Portuguese.
#ifndef MOSSLING_TEXT_H
#define MOSSLING_TEXT_H

typedef enum {
    TXT_PRESS_TO_PLAY,
    TXT_TAP_TO_PLAY,
    TXT_CONTROLS,
    TXT_GOAL,
    TXT_PAUSED,
    TXT_PRESS_TO_CONTINUE,
    TXT_TAP_TO_CONTINUE,
    TXT_LEVEL,        // takes the map number (%d)
    TXT_LEVEL_DONE,
    TXT_GAME_OVER,
    TXT_WON,
    TXT_TOTAL_SEEDS,  // takes the number of seeds (%d)
    TXT_PORTAL_OPEN,
    TXT_COUNT,
} TextId;

void text_init(void);  // picks the language of the system
const char *text_get(TextId id);

#endif
