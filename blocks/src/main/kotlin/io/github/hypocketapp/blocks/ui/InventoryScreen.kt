package io.github.hypocketapp.blocks.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import io.github.hypocketapp.blocks.R

/**
 * Whether the inventory screen is open, and the block a controller or the
 * arrow keys point at in it. [onOpenChanged] lets the game screen stop the
 * player and free the mouse pointer while it is open.
 */
class Inventory(private val onOpenChanged: (Boolean) -> Unit = {}) {
    val open = mutableStateOf(false)

    /** Index into [BuildBlock.entries]. */
    val cursor = mutableIntStateOf(0)

    fun toggle() = if (open.value) close() else show()

    fun show() {
        if (open.value) return
        open.value = true
        onOpenChanged(true)
    }

    fun close() {
        if (!open.value) return
        open.value = false
        onOpenChanged(false)
    }

    /** Moves the cursor through the grid, [COLUMNS] blocks a row, wrapping around. */
    fun move(dx: Int, dy: Int) {
        val count = BuildBlock.entries.size
        cursor.intValue = ((cursor.intValue + dx + dy * COLUMNS) % count + count) % count
    }

    /** Puts the block under the cursor in the hotbar's chosen place. */
    fun confirm(selection: BlockSelection) = selection.assign(cursor.intValue)

    companion object {
        const val COLUMNS = 6
    }
}

/**
 * The inventory: every block with how many the player carries, and the
 * hotbar. Choose a place in the hotbar, then the block to put there.
 */
@Composable
fun InventoryPanel(inventory: Inventory, selection: BlockSelection, counts: List<Int>, creative: Boolean) {
    // Covers the game: touches here never turn the camera or break blocks.
    Box(
        Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.55f)).pointerInput(Unit) {
            awaitPointerEventScope { while (true) awaitPointerEvent().changes.forEach { it.consume() } }
        },
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier.background(Color(0xEE1E1E1E), RoundedCornerShape(10.dp)).padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    stringResource(R.string.blocks_inventory),
                    color = Color.White,
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.weight(1f, fill = false).padding(end = 24.dp),
                )
                OutlinedButton(onClick = { inventory.close() }) {
                    Text(stringResource(R.string.blocks_close), color = Color.White)
                }
            }
            BuildBlock.entries.chunked(Inventory.COLUMNS).forEachIndexed { row, blocks ->
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    blocks.forEachIndexed { column, block ->
                        val i = row * Inventory.COLUMNS + column
                        val carried = counts.getOrElse(i) { 0 }
                        Column(
                            Modifier.width(64.dp).clickable {
                                inventory.cursor.intValue = i
                                selection.assign(i)
                            },
                            horizontalAlignment = Alignment.CenterHorizontally,
                        ) {
                            BlockTile(
                                block,
                                size = 44.dp,
                                highlighted = inventory.cursor.intValue == i,
                                count = if (creative) null else carried,
                            )
                            Text(
                                stringResource(block.label),
                                color = Color.White.copy(alpha = if (creative || carried > 0) 1f else 0.5f),
                                style = MaterialTheme.typography.labelSmall,
                                textAlign = TextAlign.Center,
                                maxLines = 2,
                            )
                        }
                    }
                }
            }
            HotbarRow(selection, counts, creative)
            Text(
                stringResource(R.string.blocks_inventory_hint),
                color = Color.White.copy(alpha = 0.7f),
                style = MaterialTheme.typography.bodySmall,
                textAlign = TextAlign.Center,
            )
        }
    }
}

/** The hotbar's places; tap one to choose it. */
@Composable
fun HotbarRow(selection: BlockSelection, counts: List<Int>, creative: Boolean) {
    val selected = selection.index.intValue
    Row(
        Modifier.background(Color.Black.copy(alpha = 0.35f), RoundedCornerShape(6.dp)).padding(3.dp),
        horizontalArrangement = Arrangement.spacedBy(3.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        selection.slots.forEachIndexed { slot, blockIndex ->
            val block = BuildBlock.entries[blockIndex]
            BlockTile(
                block,
                size = if (slot == selected) 36.dp else 32.dp,
                highlighted = slot == selected,
                count = if (creative) null else counts.getOrElse(blockIndex) { 0 },
                modifier = Modifier.clickable { selection.select(slot) },
            )
        }
    }
}

/** A block's colour swatch, with how many are carried (null: unlimited). */
@Composable
fun BlockTile(block: BuildBlock, size: Dp, highlighted: Boolean, count: Int?, modifier: Modifier = Modifier) {
    val shape = RoundedCornerShape(3.dp)
    val empty = count != null && count <= 0
    Box(
        modifier.size(size)
            .border(if (highlighted) 2.dp else 1.dp, if (highlighted) Color.White else block.accent, shape)
            .padding(3.dp)
            .background(if (empty) block.color.copy(alpha = block.color.alpha * 0.3f) else block.color, shape),
        contentAlignment = Alignment.BottomEnd,
    ) {
        if (count != null && count > 0) {
            Text(
                count.toString(),
                color = Color.White,
                style = MaterialTheme.typography.labelSmall,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.background(Color.Black.copy(alpha = 0.5f), RoundedCornerShape(2.dp))
                    .padding(horizontal = 2.dp),
            )
        }
    }
}
