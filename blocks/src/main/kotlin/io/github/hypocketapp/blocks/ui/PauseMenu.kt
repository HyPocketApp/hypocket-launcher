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
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
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
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringArrayResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.hypocketapp.blocks.BlocksSettings
import io.github.hypocketapp.blocks.R
import io.github.hypocketapp.blocks.StoryNotes

/**
 * The pause menu: whether it is open and which page shows. [onOpenChanged]
 * holds the world still and frees the pointer while it is open.
 *
 * While it is open the game screen leaves keys to Android, so the menu works
 * like any other screen: the d-pad or arrow keys move between buttons, a
 * controller's A (or Enter) presses one and B (or Esc, or Back) goes back.
 */
class PauseMenu(private val onOpenChanged: (Boolean) -> Unit = {}) {
    enum class Page { MAIN, SETTINGS, NOTES, NOTE }

    val open = mutableStateOf(false)
    val page = mutableStateOf(Page.MAIN)

    /** The note shown on [Page.NOTE], and whether it was opened from the list. */
    val note = mutableIntStateOf(0)
    private var noteFromList = false

    /** Shows a note just picked up in the world; the world stands still while it is read. */
    fun showNote(id: Int) {
        note.intValue = id
        noteFromList = false
        page.value = Page.NOTE
        if (!open.value) {
            open.value = true
            onOpenChanged(true)
        }
    }

    fun openFromList(id: Int) {
        note.intValue = id
        noteFromList = true
        page.value = Page.NOTE
    }

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

    /** Back: from a page to the menu (a note to its list, or to the game), from the menu to the game. */
    fun back() = when (page.value) {
        Page.SETTINGS, Page.NOTES -> page.value = Page.MAIN
        Page.NOTE -> if (noteFromList) page.value = Page.NOTES else resume()
        Page.MAIN -> resume()
    }
}

@Composable
fun PausePanel(
    menu: PauseMenu,
    settings: BlocksSettings.Values,
    onSettings: (BlocksSettings.Values) -> Unit,
    onSaveAndExit: () -> Unit,
    notesFound: Int = 0,
) {
    // Covers the game: touches here never reach the touch controls.
    Box(
        Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.6f)).coverGame(),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier.background(Color(0xEE1E1E1E), RoundedCornerShape(10.dp)).padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            when (menu.page.value) {
                PauseMenu.Page.MAIN -> MainPage(menu, onSaveAndExit, StoryNotes.found(notesFound).size)
                PauseMenu.Page.SETTINGS -> SettingsPage(menu, settings, onSettings)
                PauseMenu.Page.NOTES -> NotesPage(menu, notesFound)
                PauseMenu.Page.NOTE -> NotePage(menu)
            }
        }
    }
}

@Composable
private fun MainPage(menu: PauseMenu, onSaveAndExit: () -> Unit, notes: Int) {
    val first = remember { FocusRequester() }
    Text(stringResource(R.string.blocks_paused), color = Color.White, style = MaterialTheme.typography.titleLarge)
    Button(onClick = { menu.resume() }, modifier = Modifier.width(240.dp).focusRequester(first)) {
        Text(stringResource(R.string.blocks_resume))
    }
    OutlinedButton(onClick = { menu.page.value = PauseMenu.Page.SETTINGS }, modifier = Modifier.width(240.dp)) {
        Text(stringResource(R.string.blocks_settings), color = Color.White)
    }
    if (notes > 0) {
        OutlinedButton(onClick = { menu.page.value = PauseMenu.Page.NOTES }, modifier = Modifier.width(240.dp)) {
            Text(stringResource(R.string.blocks_notes, notes, StoryNotes.ALL.size), color = Color.White)
        }
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

/** The notes found so far, to read again. */
@Composable
private fun NotesPage(menu: PauseMenu, mask: Int) {
    val first = remember { FocusRequester() }
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(
            stringResource(R.string.blocks_notes_title),
            color = Color.White,
            style = MaterialTheme.typography.titleLarge,
            modifier = Modifier.padding(end = 24.dp),
        )
        OutlinedButton(onClick = { menu.back() }, modifier = Modifier.focusRequester(first)) {
            Text(stringResource(R.string.blocks_back), color = Color.White)
        }
    }
    Column(
        Modifier.heightIn(max = 240.dp).verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        StoryNotes.found(mask).forEach { note ->
            OutlinedButton(onClick = { menu.openFromList(note.id) }, modifier = Modifier.width(300.dp)) {
                Text(
                    stringResource(if (note.kind == StoryNotes.Kind.PHONE) R.string.blocks_note_phone else R.string.blocks_note_paper, stringResource(note.title)),
                    color = Color.White,
                )
            }
        }
    }
    LaunchedEffect(Unit) { runCatching { first.requestFocus() } }
}

/** A note being read: a sheet of paper, or a phone's last conversation. */
@Composable
private fun NotePage(menu: PauseMenu) {
    val note = StoryNotes.of(menu.note.intValue) ?: return
    val first = remember { FocusRequester() }
    val scroll = rememberScrollState()
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(
            stringResource(note.title),
            color = Color.White,
            style = MaterialTheme.typography.titleMedium,
            modifier = Modifier.width(300.dp).padding(end = 12.dp),
        )
        OutlinedButton(onClick = { menu.back() }, modifier = Modifier.focusRequester(first)) {
            Text(stringResource(R.string.blocks_close), color = Color.White)
        }
    }
    if (note.kind == StoryNotes.Kind.PAPER) {
        Text(
            stringResource(note.text),
            color = Color(0xFF2B2418),
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.width(440.dp).heightIn(max = 230.dp).background(Color(0xFFEDE4CC), RoundedCornerShape(4.dp))
                .verticalScroll(scroll).padding(14.dp),
        )
    } else {
        Column(
            Modifier.width(440.dp).heightIn(max = 230.dp).background(Color(0xFF101418), RoundedCornerShape(12.dp))
                .verticalScroll(scroll).padding(10.dp),
            verticalArrangement = Arrangement.spacedBy(5.dp),
        ) {
            stringArrayResource(note.chat).forEach { item ->
                val (sender, text) = StoryNotes.message(item)
                val mine = sender.isEmpty()
                Column(
                    Modifier.fillMaxWidth(),
                    horizontalAlignment = if (mine) Alignment.End else Alignment.Start,
                ) {
                    if (!mine) Text(sender, color = Color(0xFF8FA3B8), style = MaterialTheme.typography.labelSmall)
                    Text(
                        text,
                        color = Color.White,
                        style = MaterialTheme.typography.bodyMedium,
                        modifier = Modifier.widthIn(max = 320.dp)
                            .background(if (mine) Color(0xFF2F5D8A) else Color(0xFF2C3238), RoundedCornerShape(10.dp))
                            .padding(horizontal = 10.dp, vertical = 5.dp),
                    )
                }
            }
        }
    }
    // A controller or keyboard scrolls the note with the d-pad once Close has focus.
    LaunchedEffect(Unit) { runCatching { first.requestFocus() } }
}
