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
    [TXT_CONTROLS] = "WASD or arrows: walk   Space: swing   P: pause",
    [TXT_GOAL] = "Collect every seed to open the portal",
    [TXT_PAUSED] = "Paused",
    [TXT_PRESS_TO_CONTINUE] = "Press Enter to continue",
    [TXT_TAP_TO_CONTINUE] = "Tap to continue",
    [TXT_LEVEL] = "Map %d",
    [TXT_LEVEL_DONE] = "Map complete!",
    [TXT_GAME_OVER] = "You fainted",
    [TXT_WON] = "You made it!",
    [TXT_TOTAL_SEEDS] = "Seeds collected: %d",
    [TXT_PORTAL_OPEN] = "The portal is open!",
};

static const char *const portuguese[TXT_COUNT] = {
    [TXT_PRESS_TO_PLAY] = "Aperte Enter para jogar",
    [TXT_TAP_TO_PLAY] = "Toque para jogar",
    [TXT_CONTROLS] = "WASD ou setas: andar   Espaço: atacar   P: pausa",
    [TXT_GOAL] = "Colete todas as sementes para abrir o portal",
    [TXT_PAUSED] = "Pausado",
    [TXT_PRESS_TO_CONTINUE] = "Aperte Enter para continuar",
    [TXT_TAP_TO_CONTINUE] = "Toque para continuar",
    [TXT_LEVEL] = "Mapa %d",
    [TXT_LEVEL_DONE] = "Mapa concluído!",
    [TXT_GAME_OVER] = "Você desmaiou",
    [TXT_WON] = "Você conseguiu!",
    [TXT_TOTAL_SEEDS] = "Sementes coletadas: %d",
    [TXT_PORTAL_OPEN] = "O portal se abriu!",
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
