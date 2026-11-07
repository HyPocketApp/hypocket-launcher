package io.github.hypocketapp.launcher.ui.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.hypocketapp.core.settings.LauncherSettings
import io.github.hypocketapp.core.settings.SettingsStore
import io.github.hypocketapp.engine.Engine
import io.github.hypocketapp.launcher.R
import io.github.hypocketapp.launcher.ui.ScreenColumn
import kotlin.math.roundToInt

@Composable
fun SettingsScreen(store: SettingsStore, modifier: Modifier = Modifier) {
    val settings by store.settings.collectAsStateWithLifecycle()
    val range = LauncherSettings.RENDER_SCALE_RANGE

    ScreenColumn(stringResource(R.string.settings_title), modifier) {
        Text(stringResource(R.string.settings_render_scale, settings.renderScalePercent))
        Slider(
            value = settings.renderScalePercent.toFloat(),
            onValueChange = { value -> store.update { it.copy(renderScalePercent = value.roundToInt()) } },
            valueRange = range.first.toFloat()..range.last.toFloat(),
            steps = (range.last - range.first) / 5 - 1,
        )

        Text(stringResource(R.string.settings_fps_limit))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            LauncherSettings.FPS_LIMITS.forEach { fps ->
                FilterChip(
                    selected = settings.fpsLimit == fps,
                    onClick = { store.update { it.copy(fpsLimit = fps) } },
                    label = { Text(fps.toString()) },
                )
            }
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(stringResource(R.string.settings_low_memory), modifier = Modifier.weight(1f))
            Switch(
                checked = settings.lowMemoryMode,
                onCheckedChange = { checked -> store.update { it.copy(lowMemoryMode = checked) } },
            )
        }

        Text(
            stringResource(R.string.settings_engine_info, Engine.version, Engine.abi),
            style = MaterialTheme.typography.bodySmall,
        )
        TranslatorTestSection()

        GraphicsTestSection()

        Text(stringResource(R.string.unofficial_notice), style = MaterialTheme.typography.bodySmall)
    }
}
