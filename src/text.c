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
    [TXT_PRESS_TO_PLAY] = "Press Enter to play",
    [TXT_TAP_TO_PLAY] = "Tap to play",
    [TXT_CONTROLS] = "WASD: walk   Space: act   X: back   P: pause",
    [TXT_GOAL] = "Clear the way with your axe and calm the Elder Stump",
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
    [TXT_TOAST_REST] = "You rest by the fire. Fully healed.",
    [TXT_TOAST_TOO_HARD] = "Too hard for the axe.",
    [TXT_MENU_ATTACK] = "Axe Strike",
    [TXT_MENU_SKILLS] = "Techniques",
    [TXT_MENU_HERB] = "Herb (%d)",
    [TXT_MENU_DEFEND] = "Defend",
    [TXT_MENU_FLEE] = "Run",
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
    [TXT_MSG_EMBERS] = "Fire beings surround the woman in white!",
    [TXT_MSG_EMBERS_TAUNT] = "%s: \"Her power will be ours!\"",
    [TXT_SCENE_KEYS] = "Enter: next   X: skip",
    [TXT_SCENE_TAP] = "Tap to go on",
    [TXT_NAME_STRANGER] = "???",
    [TXT_NAME_GODDESS] = "Moon Goddess",
    [TXT_INTRO_1] = "At the edge of the forest lives a lumberjack.\nJust him, his axe and the sound of the trees.",
    [TXT_INTRO_2] = "One day, he decides to go to a nearby village.\nHe takes his axe and sets off down the trail.",
    [TXT_INTRO_3] = "On the way, a cry rings out\nfrom between the trees...",
    [TXT_GODDESS_1] = "The majestic woman turns to you.\nHer hair and clothes are white as moonlight.",
    [TXT_GODDESS_2] = "Thank you, lumberjack.\nYou came just in time.",
    [TXT_GODDESS_3] = "I am the Goddess of the Moon.\nBut I have lost most of my powers.",
    [TXT_GODDESS_4] = "Those fire beings are of a race\nthat wants to steal my power.",
    [TXT_GODDESS_TALK] = "The fire beings want what is left of my power.\nBe careful, lumberjack.",
};

static const char *const portuguese[TXT_COUNT] = {
    [TXT_PRESS_TO_PLAY] = "Aperte Enter para jogar",
    [TXT_TAP_TO_PLAY] = "Toque para jogar",
    [TXT_CONTROLS] = "WASD: andar   Espaço: agir   X: voltar   P: pausa",
    [TXT_GOAL] = "Abra caminho com o machado e acalme o Toco Ancião",
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
    [TXT_TOAST_REST] = "Você descansa na fogueira. Tudo recuperado.",
    [TXT_TOAST_TOO_HARD] = "Duro demais para o machado.",
    [TXT_MENU_ATTACK] = "Machadada",
    [TXT_MENU_SKILLS] = "Técnicas",
    [TXT_MENU_HERB] = "Erva (%d)",
    [TXT_MENU_DEFEND] = "Defender",
    [TXT_MENU_FLEE] = "Fugir",
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
    [TXT_MSG_EMBERS] = "Seres de fogo cercam a mulher de branco!",
    [TXT_MSG_EMBERS_TAUNT] = "%s: \"O poder dela será nosso!\"",
    [TXT_SCENE_KEYS] = "Enter: seguir   X: pular",
    [TXT_SCENE_TAP] = "Toque para seguir",
    [TXT_NAME_STRANGER] = "???",
    [TXT_NAME_GODDESS] = "Deusa da Lua",
    [TXT_INTRO_1] = "Na beira da floresta vive um lenhador.\nSó ele, o machado e o som das árvores.",
    [TXT_INTRO_2] = "Um dia, ele decide ir até uma vila próxima.\nPega o machado e segue pela trilha.",
    [TXT_INTRO_3] = "No caminho, um grito ecoa\nentre as árvores...",
    [TXT_GODDESS_1] = "A mulher majestosa se vira para você.\nCabelos e roupas brancos como a luz da lua.",
    [TXT_GODDESS_2] = "Obrigada, lenhador.\nVocê chegou na hora certa.",
    [TXT_GODDESS_3] = "Eu sou a deusa da lua.\nMas perdi grande parte dos meus poderes.",
    [TXT_GODDESS_4] = "Aqueles seres de fogo são de uma raça\nque quer roubar o meu poder.",
    [TXT_GODDESS_TALK] = "Os seres de fogo querem o que resta do meu poder.\nTome cuidado, lenhador.",
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

const char *text_get(TextId id)
{
    return (id >= 0 && id < TXT_COUNT) ? current[id] : "";
}
