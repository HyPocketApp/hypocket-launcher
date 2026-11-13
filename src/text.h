// Words shown to the player, in English and Brazilian Portuguese.
#ifndef TIMBERHEART_TEXT_H
#define TIMBERHEART_TEXT_H

typedef enum {
    // Screens
    TXT_PRESS_TO_PLAY,
    TXT_TAP_TO_PLAY,
    TXT_CONTROLS,
    TXT_GOAL,
    TXT_PAUSED,
    TXT_PRESS_TO_CONTINUE,
    TXT_TAP_TO_CONTINUE,
    TXT_GAME_OVER,
    TXT_WAKE_UP,
    TXT_WON,
    TXT_WON_2,
    // Map
    TXT_LEVEL_SHORT,    // %d: level
    TXT_MAP_1,
    TXT_MAP_2,
    TXT_MAP_3,
    TXT_TOAST_HERBS,    // %d
    TXT_TOAST_AXE,      // %d
    TXT_TOAST_REST,
    TXT_TOAST_TOO_HARD,
    // Battle menus and status
    TXT_MENU_ATTACK,
    TXT_MENU_SKILLS,
    TXT_MENU_HERB,      // %d: herbs
    TXT_MENU_DEFEND,
    TXT_MENU_FLEE,
    TXT_SKILL_SPLIT,    // %d: cost
    TXT_SKILL_SPIN,     // %d: cost
    TXT_BACK,
    TXT_CHOOSE_TARGET,
    TXT_HERO,
    TXT_HP,
    TXT_SP,
    // Monsters
    TXT_SLIME,
    TXT_BEETLE,
    TXT_WOLF,
    TXT_ELDER_STUMP,
    // Battle messages; %s is a monster, %d a number
    TXT_MSG_START,
    TXT_MSG_BOSS,
    TXT_MSG_CRIT,       // put before a hit message
    TXT_MSG_ATTACK,     // %s %d
    TXT_MSG_SPLIT,      // %s %d
    TXT_MSG_SPIN,
    TXT_MSG_SPIN_HIT,   // %s %d
    TXT_MSG_HERB,       // %d
    TXT_MSG_DEFEND,
    TXT_MSG_FLEE_OK,
    TXT_MSG_FLEE_FAIL,
    TXT_MSG_CANT_FLEE,
    TXT_MSG_ENEMY_ATTACK, // %s %d
    TXT_MSG_BITE,       // %s %d
    TXT_MSG_ROOTS,      // %d
    TXT_MSG_ENEMY_DOWN, // %s
    TXT_MSG_VICTORY,    // %d
    TXT_MSG_FOUND_HERB, // %d
    TXT_MSG_LEVEL_UP,   // %d
    TXT_MSG_PLAYER_DOWN,
    TXT_COUNT,
} TextId;

void text_init(void);  // picks the language of the system
const char *text_get(TextId id);

#endif
