package io.github.hypocketapp.core.settings

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/** Options that affect performance. Defaults favor low-end phones. */
data class LauncherSettings(
    /** Rendering resolution as a percentage of the screen resolution. */
    val renderScalePercent: Int = 75,
    val fpsLimit: Int = 30,
    /** Lowers the 3D resolution while the GPU cannot keep up (HyPocket Blocks for now). */
    val dynamicResolution: Boolean = true,
    /** How fast HyPocket's touch, controller and mouse controls turn the camera. */
    val lookSensitivityPercent: Int = 100,
    /** Lowers memory use at the cost of more loading. */
    val lowMemoryMode: Boolean = true,
    /** Avoids using the player's mobile data for the large game download. */
    val downloadOnWifiOnly: Boolean = true,
) {
    companion object {
        val RENDER_SCALE_RANGE = 50..100
        val FPS_LIMITS = listOf(30, 45, 60)
        val LOOK_SENSITIVITY_RANGE = 25..400
    }
}

/** Saves [LauncherSettings] in the app's private shared preferences. */
class SettingsStore(context: Context) {
    private val prefs = context.applicationContext
        .getSharedPreferences("launcher_settings", Context.MODE_PRIVATE)

    private val state = MutableStateFlow(read())
    val settings: StateFlow<LauncherSettings> = state.asStateFlow()

    fun update(transform: (LauncherSettings) -> LauncherSettings) {
        val updated = sanitize(transform(state.value))
        prefs.edit()
            .putInt(KEY_RENDER_SCALE, updated.renderScalePercent)
            .putInt(KEY_FPS_LIMIT, updated.fpsLimit)
            .putBoolean(KEY_DYNAMIC_RESOLUTION, updated.dynamicResolution)
            .putInt(KEY_LOOK_SENSITIVITY, updated.lookSensitivityPercent)
            .putBoolean(KEY_LOW_MEMORY, updated.lowMemoryMode)
            .putBoolean(KEY_WIFI_ONLY, updated.downloadOnWifiOnly)
            .apply()
        state.value = updated
    }

    private fun read(): LauncherSettings {
        val defaults = LauncherSettings()
        return sanitize(
            LauncherSettings(
                renderScalePercent = prefs.getInt(KEY_RENDER_SCALE, defaults.renderScalePercent),
                fpsLimit = prefs.getInt(KEY_FPS_LIMIT, defaults.fpsLimit),
                dynamicResolution = prefs.getBoolean(KEY_DYNAMIC_RESOLUTION, defaults.dynamicResolution),
                lookSensitivityPercent = prefs.getInt(KEY_LOOK_SENSITIVITY, defaults.lookSensitivityPercent),
                lowMemoryMode = prefs.getBoolean(KEY_LOW_MEMORY, defaults.lowMemoryMode),
                downloadOnWifiOnly = prefs.getBoolean(KEY_WIFI_ONLY, defaults.downloadOnWifiOnly),
            ),
        )
    }

    private fun sanitize(settings: LauncherSettings) = settings.copy(
        renderScalePercent = settings.renderScalePercent.coerceIn(LauncherSettings.RENDER_SCALE_RANGE),
        lookSensitivityPercent = settings.lookSensitivityPercent.coerceIn(LauncherSettings.LOOK_SENSITIVITY_RANGE),
        fpsLimit = settings.fpsLimit.takeIf { it in LauncherSettings.FPS_LIMITS }
            ?: LauncherSettings.FPS_LIMITS.first(),
    )

    private companion object {
        const val KEY_RENDER_SCALE = "render_scale_percent"
        const val KEY_FPS_LIMIT = "fps_limit"
        const val KEY_DYNAMIC_RESOLUTION = "dynamic_resolution"
        const val KEY_LOOK_SENSITIVITY = "look_sensitivity_percent"
        const val KEY_LOW_MEMORY = "low_memory_mode"
        const val KEY_WIFI_ONLY = "download_on_wifi_only"
    }
}
