package io.github.hypocketapp.blocks

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit

/**
 * The settings changed from the game's own settings screen, kept in the
 * game's preferences. A setting never changed in the game follows the host
 * app's value ([BlocksConfig]), so the launcher's Settings still work until
 * the player picks something in the game.
 */
class BlocksSettings(private val prefs: SharedPreferences, private val config: BlocksConfig) {

    data class Values(
        /** Camera speed; 100 is the default. */
        val sensitivityPercent: Int,
        /** Blocks; 0 for the mode's default (72, or 128 in heavy mode). */
        val viewDistance: Int,
        /** The 3D scene's resolution, as a share of the screen's (50-100). */
        val scalePercent: Int,
        val volumePercent: Int,
        /** 0: no limit. */
        val fpsLimit: Int,
        /** The frame rate and other figures in the corner. */
        val showStats: Boolean,
    )

    fun load(): Values = Values(
        sensitivityPercent = prefs.getInt(KEY_SENSITIVITY, config.lookSensitivityPercent),
        viewDistance = prefs.getInt(KEY_VIEW, 0),
        scalePercent = prefs.getInt(KEY_SCALE, 100),
        volumePercent = prefs.getInt(KEY_VOLUME, 100),
        fpsLimit = prefs.getInt(KEY_FPS, config.fpsLimit),
        showStats = prefs.getBoolean(KEY_STATS, true),
    )

    /** Keeps only what differs from [before], so untouched settings keep following the app. */
    fun save(before: Values, after: Values) = prefs.edit {
        if (after.sensitivityPercent != before.sensitivityPercent) putInt(KEY_SENSITIVITY, after.sensitivityPercent)
        if (after.viewDistance != before.viewDistance) putInt(KEY_VIEW, after.viewDistance)
        if (after.scalePercent != before.scalePercent) putInt(KEY_SCALE, after.scalePercent)
        if (after.volumePercent != before.volumePercent) putInt(KEY_VOLUME, after.volumePercent)
        if (after.fpsLimit != before.fpsLimit) putInt(KEY_FPS, after.fpsLimit)
        if (after.showStats != before.showStats) putBoolean(KEY_STATS, after.showStats)
    }

    companion object {
        /** The game's own preferences (shared with the home screen's test options). */
        const val PREFS = "hypocket_blocks"

        val SENSITIVITIES = listOf(50, 75, 100, 150, 200)
        val VIEW_DISTANCES = listOf(0, 48, 72, 96, 128)
        val SCALES = listOf(100, 85, 70, 50)
        val VOLUMES = listOf(0, 25, 50, 75, 100)
        val FPS_LIMITS = listOf(30, 45, 60, 0)

        private const val KEY_SENSITIVITY = "settings.sensitivity"
        private const val KEY_VIEW = "settings.view"
        private const val KEY_SCALE = "settings.scale"
        private const val KEY_VOLUME = "settings.volume"
        private const val KEY_FPS = "settings.fps"
        private const val KEY_STATS = "settings.stats"

        fun of(context: Context, config: BlocksConfig) =
            BlocksSettings(context.getSharedPreferences(PREFS, Context.MODE_PRIVATE), config)
    }
}
