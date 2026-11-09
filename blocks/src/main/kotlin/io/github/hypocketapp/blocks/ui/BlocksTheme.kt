package io.github.hypocketapp.blocks.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

/** The game's own screens: dark, with the grass and plank colours of the world. */
private val Colors = darkColorScheme(
    primary = Color(0xFF7BC96F),
    secondary = Color(0xFFD9A75F),
    background = Color(0xFF101410),
    surface = Color(0xFF161B16),
)

@Composable
fun BlocksTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = Colors, content = content)
}
