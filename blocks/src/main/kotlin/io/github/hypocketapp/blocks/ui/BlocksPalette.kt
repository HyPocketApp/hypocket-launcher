package io.github.hypocketapp.blocks.ui

import androidx.annotation.StringRes
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.ui.graphics.Color
import io.github.hypocketapp.blocks.BlocksStats
import io.github.hypocketapp.blocks.R

/**
 * What the player carries, in the game's order (palette[] in
 * blocks/game/blocks.c): the index is what the game receives. Blocks come
 * first, then items, which are never placed: tools, food and materials.
 * The colours are close to the game's generated textures, for the hotbar.
 */
enum class BuildBlock(
    @param:StringRes val label: Int,
    val color: Color,
    val accent: Color,
    val kind: Kind = Kind.BLOCK,
) {
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
    SHADE(R.string.block_shade, Color(0xFF2B2138), Color(0xFF6B4D9E)),
    KILN(R.string.block_kiln, Color(0xFF8F7866), Color(0xFFFF7A29)),
    STOVE(R.string.block_stove, Color(0xFF3D3D42), Color(0xFFA6A19A)),
    CAMPFIRE(R.string.block_campfire, Color(0xFFFF8C26), Color(0xFF66472A)),
    STICK(R.string.item_stick, Color(0xFF785430), Color(0xFF4A331C), Kind.MATERIAL),
    FLINT(R.string.item_flint, Color(0xFF4D4D57), Color(0xFF8A8A94), Kind.MATERIAL),
    KNIFE(R.string.item_knife, Color(0xFFD1D6DE), Color(0xFF73492B), Kind.TOOL),
    SAW(R.string.item_saw, Color(0xFFC7CCD4), Color(0xFF8A5A33), Kind.TOOL),
    AXE(R.string.item_axe, Color(0xFF8C8C94), Color(0xFF785430), Kind.TOOL),
    PICK(R.string.item_pick, Color(0xFF7A7A82), Color(0xFF785430), Kind.TOOL),
    BERRIES(R.string.item_berries, Color(0xFFB8334D), Color(0xFF33732E), Kind.FOOD),
    RAW_MEAT(R.string.item_raw_meat, Color(0xFFD9606B), Color(0xFFF2D9D0), Kind.FOOD),
    COOKED_MEAT(R.string.item_cooked_meat, Color(0xFF8C4A2B), Color(0xFF5C2E1A), Kind.FOOD),
    FRIED_MEAT(R.string.item_fried_meat, Color(0xFFC7802E), Color(0xFF8C5420), Kind.FOOD),
    FAT(R.string.item_fat, Color(0xFFF2EBCC), Color(0xFFD9CFA6), Kind.MATERIAL),
    HIDE(R.string.item_hide, Color(0xFF9E6B40), Color(0xFF6B4526), Kind.MATERIAL),
    WOOL(R.string.item_wool, Color(0xFFF0F0EB), Color(0xFFC9C9C2), Kind.MATERIAL),
    COAT(R.string.item_coat, Color(0xFF8C5E38), Color(0xFFEDEDE6), Kind.CLOTHING),
    ;

    enum class Kind {
        /** Placed in the world. */
        BLOCK,

        /** Used up in recipes. */
        MATERIAL,

        /** Carried, it makes things possible (recipes, breaking stone, hunting) and is never used up. */
        TOOL,

        /** Eaten with the place action. */
        FOOD,

        /** Worn while carried. */
        CLOTHING,
    }

    /** Worth a place in the hotbar: something to place or to eat. */
    val usable: Boolean get() = kind == Kind.BLOCK || kind == Kind.FOOD

    /** The SURVIVAL_NEAR_* bit of a workstation (0 for anything else), as in [BlocksStats.survivalFlags]. */
    val stationBit: Int
        get() = when (this) {
            KILN -> BlocksStats.NEAR_KILN
            STOVE -> BlocksStats.NEAR_STOVE
            CAMPFIRE -> BlocksStats.NEAR_CAMPFIRE
            else -> 0
        }
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
        // Tools and materials only need to be carried: they never take a place.
        if (block !in BuildBlock.entries.indices || !BuildBlock.entries[block].usable) return
        val chosen = index.intValue
        val elsewhere = slots.indexOf(block)
        if (elsewhere >= 0) slots[elsewhere] = slots[chosen]
        slots[chosen] = block
        send(block)
        onArranged(slots.toList())
    }

    /**
     * Survival worlds: a block or food the player starts carrying that has no place
     * yet takes the first place whose block has run out, so what was just
     * collected can be used without opening the inventory.
     */
    fun fill(counts: List<Int>) {
        if (creative) return
        var changed = false
        for (block in BuildBlock.entries.indices) {
            if (!BuildBlock.entries[block].usable || counts.getOrElse(block) { 0 } <= 0 || block in slots) continue
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
