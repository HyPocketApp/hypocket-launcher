// Words shown to the player, in English and Brazilian Portuguese.
#ifndef TIMBERHEART_TEXT_H
#define TIMBERHEART_TEXT_H

#include <stdbool.h>

typedef enum {
    // Screens
    TXT_CONTROLS,
    TXT_PAUSED,
    TXT_PRESS_TO_CONTINUE,
    TXT_TAP_TO_CONTINUE,
    TXT_GAME_OVER,
    TXT_WAKE_UP,
    TXT_WON,
    TXT_WON_2,
    TXT_NEW_GAME,       // title menu
    TXT_OPTIONS,
    TXT_QUIT,
    TXT_LOAD_GAME,
    TXT_HINT_KEY,       // the action button, before a hint
    TXT_HINT_TAP,
    TXT_HINT_REST,
    TXT_HINT_HOME,
    TXT_HINT_OPEN,
    TXT_SAVE_NAME,      // %d: the game's number
    TXT_LEVEL_LONG,     // %d
    TXT_LANGUAGE,       // the name of the language, in itself
    TXT_LANGUAGE_LABEL,
    TXT_SOUND_LABEL,
    TXT_ON,
    TXT_OFF,
    // Map
    TXT_LEVEL_SHORT,    // %d: level
    TXT_MAP_HOME,       // the four area names stay in this order
    TXT_MAP_1,
    TXT_MAP_2,
    TXT_MAP_3,
    TXT_TOAST_HERBS,    // %d
    TXT_TOAST_AXE,      // %d
    TXT_TOAST_REST,
    TXT_TOAST_HOME,
    TXT_TOAST_NOT_YET,
    TXT_TOAST_SAVE_FAILED,
    // Battle menus and status
    TXT_MENU_ATTACK,
    TXT_MENU_SKILLS,
    TXT_MENU_HERB,      // %d: herbs
    TXT_MENU_SPELLS,
    TXT_MENU_ITEMS,
    TXT_HERO_TURN,
    TXT_CHOOSE_ALLY,
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
    TXT_EMBER,
    TXT_ELDER_STUMP,
    // Battle messages; %s is a monster, %d a number
    TXT_MSG_START,
    TXT_MSG_BOSS,
    TXT_MSG_EMBERS,
    TXT_MSG_EMBER_FLEES,  // %s
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
    // Story scenes (the script itself is in story.c).
    TXT_SCENE_KEYS,
    TXT_SCENE_TAP,
    // The goddess in battle.
    TXT_GODDESS_SHORT,
    TXT_MP,
    TXT_ALLY_TURN,
    TXT_SPELL_BEAM,     // %d: cost
    TXT_SPELL_LIGHT,    // %d: cost
    TXT_MSG_BEAM,       // %s %d
    TXT_MSG_LIGHT,      // %d
    TXT_MSG_ALLY_DEFEND,
    TXT_MSG_ALLY_DOWN,
    TXT_MSG_ENEMY_ATTACK_ALLY, // %s %d
    TXT_MSG_BITE_ALLY,  // %s %d
    TXT_MSG_ROOTS_ALLY, // %d
    TXT_MSG_ALLY_ATTACK,  // %s %d
    TXT_MSG_HERB_HER,     // %d: she eats one
    TXT_MSG_HERB_TO_HER,  // %d: he gives her one
    TXT_MSG_HERB_TO_HIM,  // %d: she gives him one
    TXT_MSG_BOUND,
    TXT_MSG_BOUND_ALLY,
    TXT_MSG_HELD,
    TXT_MSG_HELD_ALLY,
    TXT_MSG_FLAME,        // %s %d
    TXT_MSG_FLAME_ALLY,
    TXT_MSG_HORN,         // %s %d
    TXT_MSG_HORN_ALLY,
    TXT_MSG_MISS,         // %s
    TXT_MSG_SLAM,         // %d
    TXT_MSG_SLAM_ALLY,
    TXT_MSG_ENEMY_DEFEND, // %s
    TXT_MSG_SHELL,        // %s
    TXT_MSG_HOWL,         // %s
    TXT_MSG_SAP,          // %s %d
    TXT_COUNT,
} TextId;

void text_init(void);  // picks the language of the system
const char *text_get(TextId id);
bool text_is_portuguese(void);
void text_set_portuguese(bool on);

#endif
