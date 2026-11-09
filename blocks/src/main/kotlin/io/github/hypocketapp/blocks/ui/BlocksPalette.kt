package io.github.hypocketapp.blocks.ui

import androidx.annotation.StringRes
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.ui.graphics.Color
import io.github.hypocketapp.blocks.R

/**
 * The blocks the player builds with, in the game's order (palette[] in
 * blocks/game/blocks.c): the index is what the game receives.
 * The colours are close to the game's generated textures, for the hotbar.
 */
enum class BuildBlock(@param:StringRes val label: Int, val color: Color, val accent: Color) {
    PLANKS(R.string.block_planks, Color(0xFFB38A52), Color(0xFF7A5C34)),
    STONE_BRICKS(R.string.block_stone_bricks, Color(0xFF8F8F8F), Color(0xFF5E5E5E)),
    BRICKS(R.string.block_bricks, Color(0xFFA34D3B), Color(0xFFC2B8A8)),
    GLASS(R.string.block_glass, Color(0x55C8E0F2), Color(0xFFE6EDF5)),
    ROOF(R.string.block_roof, Color(0xFFA84D30), Color(0xFF6E3220)),
    PLASTER(R.string.block_plaster, Color(0xFFE0DBCC), Color(0xFFC4BFB0)),
    LAMP(R.string.block_lamp, Color(0xFFFFDB85), Color(0xFF5A4229)),
    WOOD(R.string.block_wood, Color(0xFF6B4D2E), Color(0xFF9E7A4C)),
    STONE(R.string.block_stone, Color(0xFF858585), Color(0xFF6A6A6A)),
    DIRT(R.string.block_dirt, Color(0xFF855E3D), Color(0xFF6B4B30)),
    SAND(R.string.block_sand, Color(0xFFDBCC94), Color(0xFFC9BA82)),
    LEAVES(R.string.block_leaves, Color(0xFF33732E), Color(0xFF285C24)),
}

/**
 * The hotbar: [SLOTS] places, each holding one of the [BuildBlock]s, and the
 * chosen place. Shared by the touch hotbar, the inventory screen, controllers,
 * keyboards and mice. [send] tells the game which block to place (an index
 * into its palette) and [onArranged] keeps a new arrangement (the world's
 * settings).
 */
class BlockSelection(
    initial: List<Int>,
    private val creative: Boolean,
    private val send: (Int) -> Unit,
    private val onArranged: (List<Int>) -> Unit = {},
) {
    /** The block in each place, as an index into [BuildBlock.entries]. */
    val slots = mutableStateListOf<Int>().apply { addAll(arrangement(initial, creative)) }

    /** The chosen place, 0 until [SLOTS] - 1. */
    val index = mutableIntStateOf(0)

    /** The chosen block. */
    val block: BuildBlock get() = BuildBlock.entries[slots[index.intValue]]

    init {
        send(slots[0])
    }

    fun select(slot: Int) {
        val wrapped = ((slot % SLOTS) + SLOTS) % SLOTS
        index.intValue = wrapped
        send(slots[wrapped])
    }

    fun next() = select(index.intValue + 1)

    fun previous() = select(index.intValue - 1)

    /**
     * Puts [block] in the chosen place. If it was already in another place the
     * two swap, so every block stays reachable.
     */
    fun assign(block: Int) {
        if (block !in BuildBlock.entries.indices) return
        val chosen = index.intValue
        val elsewhere = slots.indexOf(block)
        if (elsewhere >= 0) slots[elsewhere] = slots[chosen]
        slots[chosen] = block
        send(block)
        onArranged(slots.toList())
    }

    /**
     * Survival worlds: a block the player starts carrying that has no place
     * yet takes the first place whose block has run out, so what was just
     * collected can be used without opening the inventory.
     */
    fun fill(counts: List<Int>) {
        if (creative) return
        var changed = false
        for (block in BuildBlock.entries.indices) {
            if (counts.getOrElse(block) { 0 } <= 0 || block in slots) continue
            val empty = slots.indexOfFirst { counts.getOrElse(it) { 0 } <= 0 }
            if (empty < 0) break
            slots[empty] = block
            changed = true
        }
        if (changed) {
            send(slots[index.intValue])
            onArranged(slots.toList())
        }
    }

    companion object {
        const val SLOTS = 9

        /** The saved arrangement if it is valid, else the default for the mode. */
        fun arrangement(saved: List<Int>, creative: Boolean): List<Int> {
            val valid = saved.size == SLOTS && saved.all { it in BuildBlock.entries.indices } && saved.toSet().size == SLOTS
            return when {
                valid -> saved
                creative -> BuildBlock.entries.take(SLOTS).map { it.ordinal }
                // What can be collected first, then building blocks.
                else -> listOf(
                    BuildBlock.DIRT, BuildBlock.WOOD, BuildBlock.STONE, BuildBlock.SAND, BuildBlock.LEAVES,
                    BuildBlock.PLANKS, BuildBlock.STONE_BRICKS, BuildBlock.GLASS, BuildBlock.LAMP,
                ).map { it.ordinal }
            }
        }
    }
}
