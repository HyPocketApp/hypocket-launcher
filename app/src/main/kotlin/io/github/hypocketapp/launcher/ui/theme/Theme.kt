package io.github.hypocketapp.launcher.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val Teal = Color(0xFF0E7C86)
private val TealLight = Color(0xFF6FD3DC)
private val Amber = Color(0xFFF2A541)

private val LightColors = lightColorScheme(primary = Teal, secondary = Amber)
private val DarkColors = darkColorScheme(primary = TealLight, secondary = Amber)

@Composable
fun HyPocketTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = if (isSystemInDarkTheme()) DarkColors else LightColors,
        content = content,
    )
}
