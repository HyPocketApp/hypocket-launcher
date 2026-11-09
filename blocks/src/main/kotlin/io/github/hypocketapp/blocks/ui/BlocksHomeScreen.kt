package io.github.hypocketapp.blocks.ui

import android.text.format.DateUtils
import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import io.github.hypocketapp.blocks.BlocksThumbnail
import androidx.compose.foundation.horizontalScroll
import androidx.core.content.edit
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.LifecycleResumeEffect
import io.github.hypocketapp.blocks.BlocksWorld
import io.github.hypocketapp.blocks.BlocksWorlds
import io.github.hypocketapp.blocks.R
import io.github.hypocketapp.blocks.BlocksConfig
import io.github.hypocketapp.blocks.DayMode

/**
 * HyPocket Blocks' home: the saved worlds, a quick unsaved game and the
 * benchmark. [config] is read each time a game starts, so setting changes
 * apply to the next game.
 */
@Composable
fun BlocksHomeScreen(config: () -> BlocksConfig, modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val store = remember { BlocksActivity.worlds(context) }
    var heavy by rememberSaveable { mutableStateOf(false) }
    // Kept between visits: testers set it once and play many games.
    val prefs = remember { context.getSharedPreferences(PREFS, android.content.Context.MODE_PRIVATE) }
    var dayMode by remember { mutableStateOf(DayMode.fromName(prefs.getString(KEY_DAY_MODE, null))) }
    var worlds by remember { mutableStateOf(store.list()) }
    var creating by rememberSaveable { mutableStateOf(false) }
    var deleting by remember { mutableStateOf<BlocksWorld?>(null) }
    var renaming by remember { mutableStateOf<BlocksWorld?>(null) }
    var testOptions by rememberSaveable { mutableStateOf(false) }
    val copyName = stringResource(R.string.blocks_copy_name)

    // Back from a game: the worlds' last-played times changed.
    LifecycleResumeEffect(Unit) {
        worlds = store.list()
        onPauseOrDispose {}
    }

    Column(
        modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(stringResource(R.string.blocks_title), style = MaterialTheme.typography.headlineSmall)
        Text(stringResource(R.string.blocks_description), style = MaterialTheme.typography.bodySmall)

        Text(stringResource(R.string.blocks_worlds), style = MaterialTheme.typography.titleSmall)
        if (worlds.isEmpty()) {
            Text(stringResource(R.string.blocks_no_worlds), style = MaterialTheme.typography.bodySmall)
        }
        worlds.forEach { world ->
            WorldCard(
                world,
                onPlay = { BlocksActivity.start(context, config(), heavy, world, dayMode) },
                onRename = { renaming = world },
                onDuplicate = {
                    store.duplicate(world.id, copyName.format(world.name))
                    worlds = store.list()
                },
                onDelete = { deleting = world },
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { creating = true }) { Text(stringResource(R.string.blocks_new_world)) }
            OutlinedButton(onClick = { BlocksActivity.start(context, config(), heavy, null, dayMode) }) {
                Text(stringResource(R.string.blocks_open))
            }
        }
        Text(stringResource(R.string.blocks_quick_play_description), style = MaterialTheme.typography.bodySmall)

        // For testers: the heavy load, the time of day and the benchmark.
        TextButton(onClick = { testOptions = !testOptions }) {
            Text(stringResource(if (testOptions) R.string.blocks_test_options_hide else R.string.blocks_test_options))
        }
        if (testOptions) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(stringResource(R.string.blocks_heavy), modifier = Modifier.weight(1f))
                Switch(checked = heavy, onCheckedChange = { heavy = it })
            }
            Text(stringResource(R.string.blocks_day_mode), style = MaterialTheme.typography.labelLarge)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                DayMode.entries.forEach { mode ->
                    FilterChip(
                        selected = dayMode == mode,
                        onClick = {
                            dayMode = mode
                            prefs.edit { putString(KEY_DAY_MODE, mode.name) }
                        },
                        label = { Text(stringResource(dayModeLabel(mode))) },
                    )
                }
            }
            Text(stringResource(R.string.blocks_day_mode_hint), style = MaterialTheme.typography.bodySmall)
            OutlinedButton(onClick = { BlocksActivity.startBenchmark(context, config()) }) {
                Text(stringResource(R.string.blocks_benchmark))
            }
            Text(stringResource(R.string.blocks_benchmark_description), style = MaterialTheme.typography.bodySmall)
        }
    }

    if (creating) {
        NewWorldDialog(
            onCreate = { name, chunks, creative, peaceful ->
                creating = false
                val world = store.create(name, chunks, creative, peaceful)
                worlds = store.list()
                BlocksActivity.start(context, config(), heavy, world, dayMode)
            },
            onDismiss = { creating = false },
        )
    }
    renaming?.let { world ->
        RenameDialog(
            world.name,
            onRename = { name ->
                store.rename(world.id, name)
                worlds = store.list()
                renaming = null
            },
            onDismiss = { renaming = null },
        )
    }
    deleting?.let { world ->
        AlertDialog(
            onDismissRequest = { deleting = null },
            title = { Text(stringResource(R.string.blocks_delete_title, world.name)) },
            text = { Text(stringResource(R.string.blocks_delete_text)) },
            confirmButton = {
                TextButton(onClick = {
                    store.delete(world.id)
                    worlds = store.list()
                    deleting = null
                }) { Text(stringResource(R.string.blocks_delete)) }
            },
            dismissButton = { TextButton(onClick = { deleting = null }) { Text(stringResource(R.string.blocks_cancel)) } },
        )
    }
}

@Composable
private fun WorldCard(world: BlocksWorld, onPlay: () -> Unit, onRename: () -> Unit, onDuplicate: () -> Unit, onDelete: () -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp)) {
            Row(
                Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                WorldPicture(world)
                Column(Modifier.weight(1f)) {
                    Text(world.name, style = MaterialTheme.typography.titleSmall)
                    val played = if (world.saveFile.exists()) {
                        stringResource(
                            R.string.blocks_world_played,
                            DateUtils.getRelativeTimeSpanString(world.lastPlayedMillis).toString(),
                        )
                    } else {
                        stringResource(R.string.blocks_world_new)
                    }
                    Text(
                        stringResource(
                            R.string.blocks_world_info_mode,
                            stringResource(sizeLabel(world.chunks)),
                            stringResource(
                                when {
                                    world.creative -> R.string.blocks_mode_creative
                                    world.peaceful -> R.string.blocks_mode_peaceful
                                    else -> R.string.blocks_mode_survival
                                },
                            ),
                            played,
                        ),
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
                Button(onClick = onPlay) { Text(stringResource(R.string.blocks_play)) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                TextButton(onClick = onRename) { Text(stringResource(R.string.blocks_rename)) }
                TextButton(onClick = onDuplicate) { Text(stringResource(R.string.blocks_duplicate)) }
                TextButton(onClick = onDelete) { Text(stringResource(R.string.blocks_delete)) }
            }
        }
    }
}

/** The world's last picture, or a plain tile before it was first played. */
@Composable
private fun WorldPicture(world: BlocksWorld) {
    val modifier = Modifier.size(width = 96.dp, height = 54.dp).clip(RoundedCornerShape(4.dp))
    // Read again whenever the world was played since.
    val picture = remember(world.id, world.lastPlayedMillis) {
        runCatching { BlocksThumbnail.decode(world.thumbnailFile.readBytes()) }.getOrNull()
            ?.let { Bitmap.createBitmap(it.pixels, it.width, it.height, Bitmap.Config.ARGB_8888).asImageBitmap() }
    }
    if (picture != null) {
        Image(picture, contentDescription = null, modifier = modifier, contentScale = ContentScale.Crop)
    } else {
        Box(modifier.background(MaterialTheme.colorScheme.surfaceVariant))
    }
}

@Composable
private fun RenameDialog(current: String, onRename: (String) -> Unit, onDismiss: () -> Unit) {
    var name by rememberSaveable { mutableStateOf(current) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.blocks_rename)) },
        text = {
            OutlinedTextField(
                value = name,
                onValueChange = { name = it.take(BlocksWorlds.MAX_NAME) },
                label = { Text(stringResource(R.string.blocks_world_name)) },
                singleLine = true,
            )
        },
        confirmButton = { TextButton(onClick = { onRename(name) }) { Text(stringResource(R.string.blocks_save)) } },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.blocks_cancel)) } },
    )
}

@Composable
private fun NewWorldDialog(onCreate: (String, Int, Boolean, Boolean) -> Unit, onDismiss: () -> Unit) {
    val defaultName = stringResource(R.string.blocks_default_world_name)
    var name by rememberSaveable { mutableStateOf(defaultName) }
    var chunks by rememberSaveable { mutableIntStateOf(BlocksWorlds.SIZES[1]) }
    var creative by rememberSaveable { mutableStateOf(false) }
    var peaceful by rememberSaveable { mutableStateOf(false) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.blocks_new_world)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = name,
                    onValueChange = { name = it.take(BlocksWorlds.MAX_NAME) },
                    label = { Text(stringResource(R.string.blocks_world_name)) },
                    singleLine = true,
                )
                Text(stringResource(R.string.blocks_world_size), style = MaterialTheme.typography.labelLarge)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    BlocksWorlds.SIZES.forEach { size ->
                        FilterChip(
                            selected = chunks == size,
                            onClick = { chunks = size },
                            label = { Text(stringResource(sizeLabel(size))) },
                        )
                    }
                }
                Text(stringResource(R.string.blocks_world_size_hint), style = MaterialTheme.typography.bodySmall)
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(stringResource(R.string.blocks_creative), modifier = Modifier.weight(1f))
                    Switch(checked = creative, onCheckedChange = { creative = it })
                }
                Text(
                    stringResource(if (creative) R.string.blocks_creative_on else R.string.blocks_creative_off),
                    style = MaterialTheme.typography.bodySmall,
                )
                if (!creative) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(stringResource(R.string.blocks_peaceful), modifier = Modifier.weight(1f))
                        Switch(checked = peaceful, onCheckedChange = { peaceful = it })
                    }
                    Text(
                        stringResource(if (peaceful) R.string.blocks_peaceful_on else R.string.blocks_peaceful_off),
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }
        },
        confirmButton = {
            TextButton(onClick = { onCreate(name, chunks, creative, peaceful && !creative) }) {
                Text(stringResource(R.string.blocks_create))
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.blocks_cancel)) } },
    )
}

private const val PREFS = io.github.hypocketapp.blocks.BlocksSettings.PREFS
private const val KEY_DAY_MODE = "day_mode"

private fun dayModeLabel(mode: DayMode): Int = when (mode) {
    DayMode.CYCLE -> R.string.blocks_day_cycle
    DayMode.FAST -> R.string.blocks_day_fast
    DayMode.ALWAYS_DAY -> R.string.blocks_day_always_day
    DayMode.ALWAYS_NIGHT -> R.string.blocks_day_always_night
}

private fun sizeLabel(chunks: Int): Int = when (chunks) {
    8 -> R.string.blocks_size_small
    16 -> R.string.blocks_size_medium
    else -> R.string.blocks_size_large
}
