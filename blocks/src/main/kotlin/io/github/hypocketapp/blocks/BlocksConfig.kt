package io.github.hypocketapp.blocks

import android.content.Intent

/**
 * What the app hosting HyPocket Blocks passes to it: the player's settings
 * and the app's version for reports. Keeping these here, instead of reading
 * the launcher's settings, lets the game move to its own app later.
 */
data class BlocksConfig(
    /** "App: 0.1.0 (commit abcdef12)", the first lines of reports. */
    val versionLine: String = "App: unknown",
    /** Camera speed for touch, controllers and mice; 100 is the default. */
    val lookSensitivityPercent: Int = 100,
    /** Share of the screen's width and height the game renders at. */
    val renderScalePercent: Int = 100,
    /** 0: no limit. */
    val fpsLimit: Int = 60,
    /** Lower the 3D resolution while the GPU cannot keep up. */
    val dynamicResolution: Boolean = true,
) {
    internal fun putInto(intent: Intent): Intent = intent
        .putExtra(EXTRA_VERSION, versionLine)
        .putExtra(EXTRA_SENSITIVITY, lookSensitivityPercent)
        .putExtra(EXTRA_SCALE, renderScalePercent)
        .putExtra(EXTRA_FPS, fpsLimit)
        .putExtra(EXTRA_DYNAMIC, dynamicResolution)

    internal companion object {
        private const val EXTRA_VERSION = "config.version"
        private const val EXTRA_SENSITIVITY = "config.sensitivity"
        private const val EXTRA_SCALE = "config.scale"
        private const val EXTRA_FPS = "config.fps"
        private const val EXTRA_DYNAMIC = "config.dynamic"

        fun from(intent: Intent): BlocksConfig {
            val defaults = BlocksConfig()
            return BlocksConfig(
                versionLine = intent.getStringExtra(EXTRA_VERSION) ?: defaults.versionLine,
                lookSensitivityPercent = intent.getIntExtra(EXTRA_SENSITIVITY, defaults.lookSensitivityPercent),
                renderScalePercent = intent.getIntExtra(EXTRA_SCALE, defaults.renderScalePercent),
                fpsLimit = intent.getIntExtra(EXTRA_FPS, defaults.fpsLimit),
                dynamicResolution = intent.getBooleanExtra(EXTRA_DYNAMIC, defaults.dynamicResolution),
            )
        }
    }
}
