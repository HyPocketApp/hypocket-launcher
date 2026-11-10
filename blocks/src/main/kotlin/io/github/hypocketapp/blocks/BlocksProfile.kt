package io.github.hypocketapp.blocks

import android.content.Context
import androidx.core.content.edit

/**
 * The player's profile, the same in every world: a name (for multiplayer,
 * later) and the protagonist's voice, female or male (story.md, "Décima
 * rodada"). The game gets them in its environment and plays the
 * protagonist's sounds from player/f or player/m.
 *
 * Read where the game starts (BlocksActivity.start, in the app's main
 * process) and handed over with the intent: the game screen has a process
 * of its own, which would not see a later change.
 */
class BlocksProfile(context: Context) {
    enum class Voice(val code: String) { FEMALE("f"), MALE("m") }

    private val prefs = context.getSharedPreferences(BlocksSettings.PREFS, Context.MODE_PRIVATE)

    var name: String
        get() = prefs.getString(KEY_NAME, "").orEmpty()
        set(value) = prefs.edit { putString(KEY_NAME, clean(value)) }

    var voice: Voice
        get() = Voice.entries.firstOrNull { it.code == prefs.getString(KEY_VOICE, null) } ?: Voice.FEMALE
        set(value) = prefs.edit { putString(KEY_VOICE, value.code) }

    companion object {
        const val MAX_NAME = 24
        private const val KEY_NAME = "profile_name"
        private const val KEY_VOICE = "profile_voice"

        /** A name as kept: one line, at most [MAX_NAME] characters, without spaces around it. */
        fun clean(name: String): String = name.replace('\n', ' ').trim().take(MAX_NAME)

        /** What the game reads from its environment. */
        fun environment(name: String, voice: String): Map<String, String> =
            buildMap {
                put("HYPOCKET_BLOCKS_VOICE", if (voice == Voice.MALE.code) Voice.MALE.code else Voice.FEMALE.code)
                if (name.isNotBlank()) put("HYPOCKET_BLOCKS_PLAYER_NAME", clean(name))
            }
    }
}
