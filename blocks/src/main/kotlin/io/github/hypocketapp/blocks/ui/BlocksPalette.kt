package io.github.hypocketapp.blocks.ui

import androidx.annotation.StringRes
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.ui.graphics.Color
import io.github.hypocketapp.blocks.BlocksSession
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

/** The chosen block, shared by the hotbar, controllers, keyboards and mice. */
class BlockSelection(private val session: BlocksSession) {
    val index = mutableIntStateOf(0)

    fun select(i: Int) {
        val count = BuildBlock.entries.size
        val wrapped = ((i % count) + count) % count
        index.intValue = wrapped
        session.selectBlock(wrapped)
    }

    fun next() = select(index.intValue + 1)

    fun previous() = select(index.intValue - 1)
}
