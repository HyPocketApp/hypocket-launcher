#include "text.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_ANDROID)
#include <android/configuration.h>
#include <android_native_app_glue.h>
struct android_app *GetAndroidApp(void);  // from raylib
#elif defined(_WIN32)
__declspec(dllimport) unsigned short __stdcall GetUserDefaultUILanguage(void);
#endif

static const char *const english[TXT_COUNT] = {
    [TXT_NEW_GAME] = "New game",
    [TXT_OPTIONS] = "Options",
    [TXT_QUIT] = "Quit",
    [TXT_LOAD_GAME] = "Load game",
    [TXT_HINT_KEY] = "Space",
    [TXT_HINT_TAP] = "Tap",
    [TXT_HINT_REST] = "Rest and save",
    [TXT_HINT_HOME] = "Rest at home and save",
    [TXT_HINT_OPEN] = "Open the chest",
    [TXT_SAVE_NAME] = "Game %d",
    [TXT_LEVEL_LONG] = "Level %d",
    [TXT_LANGUAGE] = "English",
    [TXT_LANGUAGE_LABEL] = "Language",
    [TXT_SOUND_LABEL] = "Sound",
    [TXT_ON] = "On",
    [TXT_OFF] = "Off",
    [TXT_CONTROLS] = "WASD: walk   Space: act   X: back   P: pause",
    [TXT_PAUSED] = "Paused",
    [TXT_PRESS_TO_CONTINUE] = "Press Enter to continue",
    [TXT_TAP_TO_CONTINUE] = "Tap to continue",
    [TXT_GAME_OVER] = "You fainted...",
    [TXT_WAKE_UP] = "You will wake up at the last campfire",
    [TXT_WON] = "The forest is at peace!",
    [TXT_WON_2] = "The Elder Stump falls asleep again.",
    [TXT_LEVEL_SHORT] = "Lv %d",
    [TXT_MAP_1] = "The Sawmill Woods",
    [TXT_MAP_2] = "Wolf Valley",
    [TXT_MAP_3] = "The Elder Clearing",
    [TXT_TOAST_HERBS] = "Found %d herbs",
    [TXT_TOAST_AXE] = "A sharper axe head! Attack +%d",
    [TXT_TOAST_REST] = "You rest by the fire. Game saved.",
    [TXT_MENU_ATTACK] = "Attack",
    [TXT_MENU_HERB] = "Herb (%d)",
    [TXT_MENU_DEFEND] = "Defend",
    [TXT_MENU_FLEE] = "Run",
    [TXT_SKILL_STRIKE] = "Axe Strike",
    [TXT_SPELL_STRIKE] = "Light Strike",
    [TXT_SKILL_SPLIT] = "Splitting Blow  %d",
    [TXT_SKILL_SPIN] = "Whirlwind Chop  %d",
    [TXT_BACK] = "Back",
    [TXT_CHOOSE_TARGET] = "Choose a target",
    [TXT_HERO] = "Lumberjack",
    [TXT_HP] = "HP",
    [TXT_SP] = "SP",
    [TXT_SLIME] = "Slime",
    [TXT_BEETLE] = "Beetle",
    [TXT_WOLF] = "Wolf",
    [TXT_ELDER_STUMP] = "Elder Stump",
    [TXT_MSG_START] = "Monsters block the way!",
    [TXT_MSG_BOSS] = "The Elder Stump wakes up!",
    [TXT_MSG_CRIT] = "Critical! ",
    [TXT_MSG_ATTACK] = "Axe strike! %s takes %d.",
    [TXT_MSG_SPLIT] = "Splitting Blow! %s takes %d.",
    [TXT_MSG_SPIN] = "Whirlwind Chop!",
    [TXT_MSG_SPIN_HIT] = "%s takes %d.",
    [TXT_MSG_HERB] = "You eat a herb: +%d HP.",
    [TXT_MSG_DEFEND] = "You brace yourself and catch your breath.",
    [TXT_MSG_FLEE_OK] = "You got away!",
    [TXT_MSG_FLEE_FAIL] = "You could not get away!",
    [TXT_MSG_CANT_FLEE] = "There is no running from this one!",
    [TXT_MSG_ENEMY_ATTACK] = "%s attacks: %d damage.",
    [TXT_MSG_BITE] = "%s bites hard: %d damage!",
    [TXT_MSG_ROOTS] = "Roots squeeze you: %d damage!",
    [TXT_MSG_ENEMY_DOWN] = "%s is down!",
    [TXT_MSG_VICTORY] = "Victory! +%d experience.",
    [TXT_MSG_FOUND_HERB] = "You found %d herb(s).",
    [TXT_MSG_LEVEL_UP] = "Level up! You are now level %d.",
    [TXT_MSG_PLAYER_DOWN] = "The lumberjack falls...",
    [TXT_EMBER] = "Ember",
    [TXT_MSG_EMBERS] = "The fire creatures attack!",
    [TXT_MSG_EMBER_FLEES] = "%s falls back!",
    [TXT_SCENE_KEYS] = "Enter: next   X: skip",
    [TXT_SCENE_TAP] = "Tap to go on",
    [TXT_GODDESS_SHORT] = "Goddess",
    [TXT_MP] = "MP",
    [TXT_ALLY_TURN] = "The goddess's turn",
    [TXT_SPELL_BEAM] = "Moonbeam  %d",
    [TXT_SPELL_LIGHT] = "Moonlight  %d",
    [TXT_MSG_BEAM] = "Moonbeam! %s takes %d.",
    [TXT_MSG_LIGHT] = "Moonlight! You both heal up to %d.",
    [TXT_MSG_ALLY_DEFEND] = "The goddess guards and gathers magic.",
    [TXT_MSG_ALLY_DOWN] = "The goddess falls!",
    [TXT_MSG_ENEMY_ATTACK_ALLY] = "%s attacks the goddess: %d damage.",
    [TXT_MSG_BITE_ALLY] = "%s bites the goddess: %d damage!",
    [TXT_MSG_ROOTS_ALLY] = "Roots squeeze the goddess: %d damage!",
    [TXT_MENU_ITEMS] = "Items",
    [TXT_HERO_TURN] = "The lumberjack's turn",
    [TXT_CHOOSE_ALLY] = "For whom?",
    [TXT_MSG_ALLY_ATTACK] = "The goddess strikes! %s takes %d.",
    [TXT_MSG_HERB_HER] = "The goddess eats a herb: +%d HP.",
    [TXT_MSG_HERB_TO_HER] = "You give the goddess a herb: +%d HP.",
    [TXT_MSG_HERB_TO_HIM] = "The goddess gives you a herb: +%d HP.",
    [TXT_MSG_BOUND] = "The roots hold you: you cannot move!",
    [TXT_MSG_BOUND_ALLY] = "The roots hold the goddess: she cannot move!",
    [TXT_MSG_HELD] = "Roots wrap around you!",
    [TXT_MSG_HELD_ALLY] = "Roots wrap around the goddess!",
    [TXT_MSG_FLAME] = "%s spits fire: %d damage!",
    [TXT_MSG_FLAME_ALLY] = "%s spits fire at the goddess: %d!",
    [TXT_MSG_HORN] = "%s charges with its horn: %d damage!",
    [TXT_MSG_HORN_ALLY] = "%s charges the goddess: %d damage!",
    [TXT_MSG_MISS] = "%s charges... and misses!",
    [TXT_MSG_SLAM] = "The Elder Stump crushes you: %d damage!",
    [TXT_MSG_SLAM_ALLY] = "The Elder Stump crushes the goddess: %d!",
    [TXT_MSG_ENEMY_DEFEND] = "%s braces itself.",
    [TXT_MSG_SHELL] = "%s hardens its shell!",
    [TXT_MSG_HOWL] = "%s howls! The pack grows fierce.",
    [TXT_MSG_SAP] = "%s drinks sap from the earth: +%d HP.",
    [TXT_MAP_HOME] = "The Lumberjack's Home",
    [TXT_TOAST_HOME] = "You rest at home. Game saved.",
    [TXT_TOAST_NOT_YET] = "Someone cried for help. Find her first.",
    [TXT_TOAST_SAVE_FAILED] = "Could not save the game!",
};

static const char *const portuguese[TXT_COUNT] = {
    [TXT_NEW_GAME] = "Novo jogo",
    [TXT_OPTIONS] = "Opções",
    [TXT_QUIT] = "Sair",
    [TXT_LOAD_GAME] = "Carregar jogo",
    [TXT_HINT_KEY] = "Espaço",
    [TXT_HINT_TAP] = "Toque",
    [TXT_HINT_REST] = "Descansar e salvar",
    [TXT_HINT_HOME] = "Descansar em casa e salvar",
    [TXT_HINT_OPEN] = "Abrir o baú",
    [TXT_SAVE_NAME] = "Jogo %d",
    [TXT_LEVEL_LONG] = "Nível %d",
    [TXT_LANGUAGE] = "Português",
    [TXT_LANGUAGE_LABEL] = "Idioma",
    [TXT_SOUND_LABEL] = "Som",
    [TXT_ON] = "Ligado",
    [TXT_OFF] = "Desligado",
    [TXT_CONTROLS] = "WASD: andar   Espaço: agir   X: voltar   P: pausa",
    [TXT_PAUSED] = "Pausado",
    [TXT_PRESS_TO_CONTINUE] = "Aperte Enter para continuar",
    [TXT_TAP_TO_CONTINUE] = "Toque para continuar",
    [TXT_GAME_OVER] = "Você desmaiou...",
    [TXT_WAKE_UP] = "Você vai acordar na última fogueira",
    [TXT_WON] = "A floresta está em paz!",
    [TXT_WON_2] = "O Toco Ancião volta a dormir.",
    [TXT_LEVEL_SHORT] = "Nv %d",
    [TXT_MAP_1] = "Bosque da Serraria",
    [TXT_MAP_2] = "Vale dos Lobos",
    [TXT_MAP_3] = "Clareira Ancestral",
    [TXT_TOAST_HERBS] = "Encontrou %d ervas",
    [TXT_TOAST_AXE] = "Lâmina mais afiada! Ataque +%d",
    [TXT_TOAST_REST] = "Você descansa na fogueira. Jogo salvo.",
    [TXT_MENU_ATTACK] = "Atacar",
    [TXT_MENU_HERB] = "Erva (%d)",
    [TXT_MENU_DEFEND] = "Defender",
    [TXT_MENU_FLEE] = "Fugir",
    [TXT_SKILL_STRIKE] = "Machadada",
    [TXT_SPELL_STRIKE] = "Golpe de Luz",
    [TXT_SKILL_SPLIT] = "Golpe Rachador  %d",
    [TXT_SKILL_SPIN] = "Corte Giratório  %d",
    [TXT_BACK] = "Voltar",
    [TXT_CHOOSE_TARGET] = "Escolha o alvo",
    [TXT_HERO] = "Lenhador",
    [TXT_HP] = "PV",
    [TXT_SP] = "FÔ",
    [TXT_SLIME] = "Gosma",
    [TXT_BEETLE] = "Besouro",
    [TXT_WOLF] = "Lobo",
    [TXT_ELDER_STUMP] = "Toco Ancião",
    [TXT_MSG_START] = "Monstros bloqueiam o caminho!",
    [TXT_MSG_BOSS] = "O Toco Ancião desperta!",
    [TXT_MSG_CRIT] = "Crítico! ",
    [TXT_MSG_ATTACK] = "Machadada! %s sofre %d.",
    [TXT_MSG_SPLIT] = "Golpe Rachador! %s sofre %d.",
    [TXT_MSG_SPIN] = "Corte Giratório!",
    [TXT_MSG_SPIN_HIT] = "%s sofre %d.",
    [TXT_MSG_HERB] = "Você come uma erva: +%d PV.",
    [TXT_MSG_DEFEND] = "Você se protege e recupera o fôlego.",
    [TXT_MSG_FLEE_OK] = "Você fugiu!",
    [TXT_MSG_FLEE_FAIL] = "Não conseguiu fugir!",
    [TXT_MSG_CANT_FLEE] = "Não há como fugir desta luta!",
    [TXT_MSG_ENEMY_ATTACK] = "%s ataca: %d de dano.",
    [TXT_MSG_BITE] = "%s morde forte: %d de dano!",
    [TXT_MSG_ROOTS] = "Raízes apertam você: %d de dano!",
    [TXT_MSG_ENEMY_DOWN] = "%s caiu!",
    [TXT_MSG_VICTORY] = "Vitória! +%d de experiência.",
    [TXT_MSG_FOUND_HERB] = "Você encontrou %d erva(s).",
    [TXT_MSG_LEVEL_UP] = "Subiu de nível! Agora você é nível %d.",
    [TXT_MSG_PLAYER_DOWN] = "O lenhador cai...",
    [TXT_EMBER] = "Fagulha",
    [TXT_MSG_EMBERS] = "As criaturas de fogo atacam!",
    [TXT_MSG_EMBER_FLEES] = "%s recua!",
    [TXT_SCENE_KEYS] = "Enter: seguir   X: pular",
    [TXT_SCENE_TAP] = "Toque para seguir",
    [TXT_GODDESS_SHORT] = "Deusa",
    [TXT_MP] = "PM",
    [TXT_ALLY_TURN] = "Vez da deusa",
    [TXT_SPELL_BEAM] = "Raio Lunar  %d",
    [TXT_SPELL_LIGHT] = "Luz da Lua  %d",
    [TXT_MSG_BEAM] = "Raio Lunar! %s sofre %d.",
    [TXT_MSG_LIGHT] = "Luz da Lua! Os dois recuperam até %d.",
    [TXT_MSG_ALLY_DEFEND] = "A deusa se protege e junta magia.",
    [TXT_MSG_ALLY_DOWN] = "A deusa caiu!",
    [TXT_MSG_ENEMY_ATTACK_ALLY] = "%s ataca a deusa: %d de dano.",
    [TXT_MSG_BITE_ALLY] = "%s morde a deusa: %d de dano!",
    [TXT_MSG_ROOTS_ALLY] = "Raízes apertam a deusa: %d de dano!",
    [TXT_MENU_ITEMS] = "Itens",
    [TXT_HERO_TURN] = "Vez do lenhador",
    [TXT_CHOOSE_ALLY] = "Para quem?",
    [TXT_MSG_ALLY_ATTACK] = "A deusa golpeia! %s sofre %d.",
    [TXT_MSG_HERB_HER] = "A deusa come uma erva: +%d PV.",
    [TXT_MSG_HERB_TO_HER] = "Você dá uma erva à deusa: +%d PV.",
    [TXT_MSG_HERB_TO_HIM] = "A deusa lhe dá uma erva: +%d PV.",
    [TXT_MSG_BOUND] = "As raízes prendem você: não consegue agir!",
    [TXT_MSG_BOUND_ALLY] = "As raízes prendem a deusa: ela não consegue agir!",
    [TXT_MSG_HELD] = "Raízes se enrolam em você!",
    [TXT_MSG_HELD_ALLY] = "Raízes se enrolam na deusa!",
    [TXT_MSG_FLAME] = "%s cospe fogo: %d de dano!",
    [TXT_MSG_FLAME_ALLY] = "%s cospe fogo na deusa: %d!",
    [TXT_MSG_HORN] = "%s investe com o chifre: %d de dano!",
    [TXT_MSG_HORN_ALLY] = "%s investe na deusa: %d de dano!",
    [TXT_MSG_MISS] = "%s investe... e erra!",
    [TXT_MSG_SLAM] = "O Toco Ancião esmaga você: %d de dano!",
    [TXT_MSG_SLAM_ALLY] = "O Toco Ancião esmaga a deusa: %d!",
    [TXT_MSG_ENEMY_DEFEND] = "%s se protege.",
    [TXT_MSG_SHELL] = "%s endurece a carapaça!",
    [TXT_MSG_HOWL] = "%s uiva! A matilha fica mais feroz.",
    [TXT_MSG_SAP] = "%s bebe a seiva da terra: +%d PV.",
    [TXT_MAP_HOME] = "Casa do Lenhador",
    [TXT_TOAST_HOME] = "Você descansa em casa. Jogo salvo.",
    [TXT_TOAST_NOT_YET] = "Alguém pediu socorro. Ache-a primeiro.",
    [TXT_TOAST_SAVE_FAILED] = "Não foi possível salvar o jogo!",
};

static const char *const *current = english;

static bool system_is_portuguese(void)
{
#if defined(PLATFORM_ANDROID)
    char lang[2] = { 0 };
    struct android_app *app = GetAndroidApp();
    if (app && app->config) AConfiguration_getLanguage(app->config, lang);
    return lang[0] == 'p' && lang[1] == 't';
#else
    const char *vars[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
    for (int i = 0; i < 3; i++) {
        const char *v = getenv(vars[i]);
        if (v && *v) return strncmp(v, "pt", 2) == 0;
    }
#if defined(_WIN32)
    return (GetUserDefaultUILanguage() & 0x3ff) == 0x16;  // LANG_PORTUGUESE
#else
    return false;
#endif
#endif
}

void text_init(void)
{
    current = system_is_portuguese() ? portuguese : english;
}

bool text_is_portuguese(void) { return current == portuguese; }

void text_set_portuguese(bool on) { current = on ? portuguese : english; }

const char *text_get(TextId id)
{
    return (id >= 0 && id < TXT_COUNT) ? current[id] : "";
}
