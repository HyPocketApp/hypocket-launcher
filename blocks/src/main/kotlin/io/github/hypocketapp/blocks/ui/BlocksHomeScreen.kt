package io.github.hypocketapp.blocks.ui

import android.text.format.DateUtils
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
    var worlds by remember { mutableStateOf(store.list()) }
    var creating by rememberSaveable { mutableStateOf(false) }
    var deleting by remember { mutableStateOf<BlocksWorld?>(null) }

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
                onPlay = { BlocksActivity.start(context, config(), heavy, world) },
                onDelete = { deleting = world },
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { creating = true }) { Text(stringResource(R.string.blocks_new_world)) }
            OutlinedButton(onClick = { BlocksActivity.start(context, config(), heavy, null) }) {
                Text(stringResource(R.string.blocks_open))
            }
        }
        Text(stringResource(R.string.blocks_quick_play_description), style = MaterialTheme.typography.bodySmall)

        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(stringResource(R.string.blocks_heavy), modifier = Modifier.weight(1f))
            Switch(checked = heavy, onCheckedChange = { heavy = it })
        }
        OutlinedButton(onClick = { BlocksActivity.startBenchmark(context, config()) }) {
            Text(stringResource(R.string.blocks_benchmark))
        }
        Text(stringResource(R.string.blocks_benchmark_description), style = MaterialTheme.typography.bodySmall)
    }

    if (creating) {
        NewWorldDialog(
            onCreate = { name, chunks, creative ->
                creating = false
                val world = store.create(name, chunks, creative)
                worlds = store.list()
                BlocksActivity.start(context, config(), heavy, world)
            },
            onDismiss = { creating = false },
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
private fun WorldCard(world: BlocksWorld, onPlay: () -> Unit, onDelete: () -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Row(
            Modifier.padding(12.dp).fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
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
                        stringResource(if (world.creative) R.string.blocks_mode_creative else R.string.blocks_mode_survival),
                        played,
                    ),
                    style = MaterialTheme.typography.bodySmall,
                )
            }
            TextButton(onClick = onDelete) { Text(stringResource(R.string.blocks_delete)) }
            Button(onClick = onPlay) { Text(stringResource(R.string.blocks_play)) }
        }
    }
}

@Composable
private fun NewWorldDialog(onCreate: (String, Int, Boolean) -> Unit, onDismiss: () -> Unit) {
    val defaultName = stringResource(R.string.blocks_default_world_name)
    var name by rememberSaveable { mutableStateOf(defaultName) }
    var chunks by rememberSaveable { mutableIntStateOf(BlocksWorlds.SIZES[1]) }
    var creative by rememberSaveable { mutableStateOf(false) }
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
            }
        },
        confirmButton = {
            TextButton(onClick = { onCreate(name, chunks, creative) }) { Text(stringResource(R.string.blocks_create)) }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.blocks_cancel)) } },
    )
}

private fun sizeLabel(chunks: Int): Int = when (chunks) {
    8 -> R.string.blocks_size_small
    16 -> R.string.blocks_size_medium
    else -> R.string.blocks_size_large
}
