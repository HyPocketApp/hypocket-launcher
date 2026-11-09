package io.github.hypocketapp.blocks.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.hypocketapp.blocks.BlocksSettings
import io.github.hypocketapp.blocks.R

/**
 * The pause menu: whether it is open and which page shows. [onOpenChanged]
 * holds the world still and frees the pointer while it is open.
 *
 * While it is open the game screen leaves keys to Android, so the menu works
 * like any other screen: the d-pad or arrow keys move between buttons, a
 * controller's A (or Enter) presses one and B (or Esc, or Back) goes back.
 */
class PauseMenu(private val onOpenChanged: (Boolean) -> Unit = {}) {
    enum class Page { MAIN, SETTINGS }

    val open = mutableStateOf(false)
    val page = mutableStateOf(Page.MAIN)

    fun show() {
        if (open.value) return
        page.value = Page.MAIN
        open.value = true
        onOpenChanged(true)
    }

    fun resume() {
        if (!open.value) return
        open.value = false
        onOpenChanged(false)
    }

    /** Back: from the settings to the menu, from the menu to the game. */
    fun back() = if (page.value == Page.SETTINGS) page.value = Page.MAIN else resume()
}

@Composable
fun PausePanel(
    menu: PauseMenu,
    settings: BlocksSettings.Values,
    onSettings: (BlocksSettings.Values) -> Unit,
    onSaveAndExit: () -> Unit,
) {
    // Covers the game: touches here never reach the touch controls.
    Box(
        Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.6f)).pointerInput(Unit) {
            awaitPointerEventScope { while (true) awaitPointerEvent().changes.forEach { it.consume() } }
        },
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier.background(Color(0xEE1E1E1E), RoundedCornerShape(10.dp)).padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            when (menu.page.value) {
                PauseMenu.Page.MAIN -> MainPage(menu, onSaveAndExit)
                PauseMenu.Page.SETTINGS -> SettingsPage(menu, settings, onSettings)
            }
        }
    }
}

@Composable
private fun MainPage(menu: PauseMenu, onSaveAndExit: () -> Unit) {
    val first = remember { FocusRequester() }
    Text(stringResource(R.string.blocks_paused), color = Color.White, style = MaterialTheme.typography.titleLarge)
    Button(onClick = { menu.resume() }, modifier = Modifier.width(240.dp).focusRequester(first)) {
        Text(stringResource(R.string.blocks_resume))
    }
    OutlinedButton(onClick = { menu.page.value = PauseMenu.Page.SETTINGS }, modifier = Modifier.width(240.dp)) {
        Text(stringResource(R.string.blocks_settings), color = Color.White)
    }
    OutlinedButton(onClick = onSaveAndExit, modifier = Modifier.width(240.dp)) {
        Text(stringResource(R.string.blocks_save_and_exit), color = Color.White)
    }
    // Ready for a controller or keyboard straight away.
    LaunchedEffect(Unit) { runCatching { first.requestFocus() } }
}

@Composable
private fun SettingsPage(menu: PauseMenu, settings: BlocksSettings.Values, onSettings: (BlocksSettings.Values) -> Unit) {
    val first = remember { FocusRequester() }
    val showStats = remember(settings.showStats) { mutableStateOf(settings.showStats) }
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(
            stringResource(R.string.blocks_settings),
            color = Color.White,
            style = MaterialTheme.typography.titleLarge,
            modifier = Modifier.padding(end = 24.dp),
        )
        OutlinedButton(onClick = { menu.back() }, modifier = Modifier.focusRequester(first)) {
            Text(stringResource(R.string.blocks_back), color = Color.White)
        }
    }
    Column(
        Modifier.heightIn(max = 260.dp).verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        ChoiceRow(R.string.blocks_setting_sensitivity, BlocksSettings.SENSITIVITIES, settings.sensitivityPercent, { "$it%" }) {
            onSettings(settings.copy(sensitivityPercent = it))
        }
        ChoiceRow(R.string.blocks_setting_view, BlocksSettings.VIEW_DISTANCES, settings.viewDistance, { viewLabel(it) }) {
            onSettings(settings.copy(viewDistance = it))
        }
        ChoiceRow(R.string.blocks_setting_resolution, BlocksSettings.SCALES, settings.scalePercent, { "$it%" }) {
            onSettings(settings.copy(scalePercent = it))
        }
        ChoiceRow(R.string.blocks_setting_volume, BlocksSettings.VOLUMES, settings.volumePercent, { "$it%" }) {
            onSettings(settings.copy(volumePercent = it))
        }
        ChoiceRow(R.string.blocks_setting_fps, BlocksSettings.FPS_LIMITS, settings.fpsLimit, { fpsLabel(it) }) {
            onSettings(settings.copy(fpsLimit = it))
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(stringResource(R.string.blocks_setting_stats), color = Color.White, modifier = Modifier.width(150.dp))
            Switch(checked = showStats.value, onCheckedChange = {
                showStats.value = it
                onSettings(settings.copy(showStats = it))
            })
        }
    }
    LaunchedEffect(Unit) { runCatching { first.requestFocus() } }
}

@Composable
private fun viewLabel(blocks: Int): String =
    if (blocks == 0) stringResource(R.string.blocks_setting_default) else pluralStringResource(R.plurals.blocks_setting_blocks, blocks, blocks)

@Composable
private fun fpsLabel(limit: Int): String =
    if (limit == 0) stringResource(R.string.blocks_setting_no_limit) else limit.toString()

/** A setting with a few choices, each a chip a controller or keyboard can reach. */
@Composable
private fun ChoiceRow(label: Int, choices: List<Int>, current: Int, text: @Composable (Int) -> String, onChoose: (Int) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(stringResource(label), color = Color.White, modifier = Modifier.width(150.dp))
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            choices.forEach { choice ->
                FilterChip(
                    selected = choice == current,
                    onClick = { onChoose(choice) },
                    label = { Text(text(choice), color = Color.White) },
                )
            }
        }
    }
}
