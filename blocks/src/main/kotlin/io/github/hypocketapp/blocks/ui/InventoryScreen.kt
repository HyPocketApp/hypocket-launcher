package io.github.hypocketapp.blocks.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
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
import io.github.hypocketapp.blocks.BlocksRecipes
import io.github.hypocketapp.blocks.BlocksStats
import io.github.hypocketapp.blocks.R
import io.github.hypocketapp.blocks.Recipe

/**
 * Whether the inventory screen is open, and what a controller or the arrow
 * keys point at in it: a block of the grid (indices below the number of
 * blocks) or a recipe (after them). [onOpenChanged] lets the game screen stop
 * the player and free the mouse pointer while it is open.
 */
class Inventory(
    /** Recipes listed beside the blocks (none in creative mode). */
    private val recipes: Int = 0,
    private val onOpenChanged: (Boolean) -> Unit = {},
) {
    val open = mutableStateOf(false)

    /** A block index into [BuildBlock.entries], or [BLOCKS] plus a recipe index. */
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

    /**
     * Moves the cursor: through the grid of blocks ([COLUMNS] a row, wrapping
     * around), and right from its last column into the recipe list, which up
     * and down walk through.
     */
    fun move(dx: Int, dy: Int) {
        val c = cursor.intValue
        if (c >= BLOCKS) {
            val recipe = c - BLOCKS
            cursor.intValue = when {
                dx < 0 -> minOf(recipe, BLOCKS / COLUMNS - 1) * COLUMNS + COLUMNS - 1
                else -> BLOCKS + ((recipe + dy) % recipes + recipes) % recipes
            }
            return
        }
        if (dx > 0 && c % COLUMNS == COLUMNS - 1 && recipes > 0) {
            cursor.intValue = BLOCKS + minOf(c / COLUMNS, recipes - 1)
            return
        }
        cursor.intValue = ((c + dx + dy * COLUMNS) % BLOCKS + BLOCKS) % BLOCKS
    }

    /** Puts the block under the cursor in the hotbar, or makes the recipe under it. */
    fun confirm(selection: BlockSelection, craft: (Int) -> Unit) {
        val c = cursor.intValue
        if (c < BLOCKS) selection.assign(c) else craft(c - BLOCKS)
    }

    companion object {
        const val COLUMNS = 8
        val BLOCKS = BuildBlock.entries.size
    }
}

/**
 * The inventory: every block and item with how many the player carries, and
 * the hotbar. Choose a place in the hotbar, then the block (or food) to put
 * there. [stations]: the workstations near the player ([BlocksStats.survivalFlags]).
 */
@Composable
fun InventoryPanel(
    inventory: Inventory,
    selection: BlockSelection,
    counts: List<Int>,
    creative: Boolean,
    stations: Int,
    onCraft: (Recipe) -> Unit,
) {
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
            Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
            Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Column(
                Modifier.heightIn(max = 190.dp).verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(6.dp),
            ) {
            BuildBlock.entries.chunked(Inventory.COLUMNS).forEachIndexed { row, blocks ->
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    blocks.forEachIndexed { column, block ->
                        val i = row * Inventory.COLUMNS + column
                        val carried = counts.getOrElse(i) { 0 }
                        Column(
                            Modifier.width(54.dp).clickable {
                                inventory.cursor.intValue = i
                                selection.assign(i)
                            },
                            horizontalAlignment = Alignment.CenterHorizontally,
                        ) {
                            BlockTile(
                                block,
                                size = 38.dp,
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
            }
            HotbarRow(selection, counts, creative)
            }
            if (!creative) RecipeList(inventory, counts, stations, onCraft)
            }
            Text(
                stringResource(if (creative) R.string.blocks_inventory_hint else R.string.blocks_inventory_hint_recipes),
                color = Color.White.copy(alpha = 0.7f),
                style = MaterialTheme.typography.bodySmall,
                textAlign = TextAlign.Center,
            )
        }
    }
}

/**
 * The recipes, each with what it uses and makes, and the tool or workstation it
 * needs; tap one (or press A or Enter on it) to make it once.
 */
@Composable
private fun RecipeList(inventory: Inventory, counts: List<Int>, stations: Int, onCraft: (Recipe) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(stringResource(R.string.blocks_recipes), color = Color.White, style = MaterialTheme.typography.labelLarge)
        Column(
            Modifier.heightIn(max = 230.dp).verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            BlocksRecipes.ALL.forEachIndexed { i, recipe ->
                val possible = recipe.canMake(counts, stations)
                val pointed = inventory.cursor.intValue == Inventory.BLOCKS + i
                Row(
                    Modifier
                        .border(if (pointed) 2.dp else 1.dp, if (pointed) Color.White else Color.White.copy(alpha = 0.2f), RoundedCornerShape(6.dp))
                        .clickable {
                            inventory.cursor.intValue = Inventory.BLOCKS + i
                            if (possible) onCraft(recipe)
                        }
                        .padding(horizontal = 6.dp, vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(4.dp),
                ) {
                    recipe.inputs.forEach { (block, n) ->
                        BlockTile(block, size = 24.dp, highlighted = false, count = n)
                    }
                    Text(stringResource(R.string.blocks_recipe_arrow), color = Color.White)
                    BlockTile(recipe.output, size = 28.dp, highlighted = false, count = recipe.count)
                    Column(Modifier.width(96.dp)) {
                        Text(
                            stringResource(recipe.output.label),
                            color = Color.White.copy(alpha = if (possible) 1f else 0.45f),
                            style = MaterialTheme.typography.labelSmall,
                            maxLines = 1,
                        )
                        recipe.tool?.let { tool ->
                            Requirement(stringResource(R.string.blocks_recipe_tool, stringResource(tool.label)), recipe.hasTool(counts))
                        }
                        recipe.station?.let { station ->
                            Requirement(stringResource(R.string.blocks_recipe_station, stringResource(station.label)), recipe.hasStation(stations))
                        }
                    }
                }
            }
        }
    }
}

/** What a recipe needs besides its inputs: grey when it is there, red when it is not. */
@Composable
private fun Requirement(text: String, met: Boolean) {
    Text(
        text,
        color = if (met) Color.White.copy(alpha = 0.6f) else Color(0xFFFF8A80),
        style = MaterialTheme.typography.labelSmall,
        maxLines = 1,
    )
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

/** A block's colour swatch (round for items), with how many are carried (null: unlimited). */
@Composable
fun BlockTile(block: BuildBlock, size: Dp, highlighted: Boolean, count: Int?, modifier: Modifier = Modifier) {
    val shape = if (block.kind == BuildBlock.Kind.BLOCK) RoundedCornerShape(3.dp) else CircleShape
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
