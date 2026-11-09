package io.github.hypocketapp.blocks

import io.github.hypocketapp.blocks.ui.BuildBlock

/** One way to make blocks from others: no crafting grid, just what goes in and what comes out. */
data class Recipe(
    /** The blocks used up, each with how many (one or two kinds). */
    val inputs: List<Pair<BuildBlock, Int>>,
    val output: BuildBlock,
    val count: Int,
) {
    init {
        require(inputs.size in 1..2 && inputs.all { it.second in 1..MAX_AMOUNT } && count in 1..MAX_AMOUNT)
        require(inputs.none { it.first == output })
    }

    /** Whether [counts] (blocks carried, by [BuildBlock] index) are enough to make it once. */
    fun canMake(counts: List<Int>): Boolean = inputs.all { (block, n) -> counts.getOrElse(block.ordinal) { 0 } >= n }

    companion object {
        /** What one byte of the shared file can hold. */
        const val MAX_AMOUNT = 64
    }
}

/**
 * Every recipe, in the order the inventory lists them: the one table to change
 * to rebalance the game. The game only checks and applies what it is sent
 * (see BlocksSession.craft and craft() in blocks.c).
 */
object BlocksRecipes {
    val ALL: List<Recipe> = listOf(
        Recipe(listOf(BuildBlock.WOOD to 1), BuildBlock.PLANKS, 4),
        Recipe(listOf(BuildBlock.STONE to 1), BuildBlock.STONE_BRICKS, 1),
        Recipe(listOf(BuildBlock.SAND to 1), BuildBlock.GLASS, 1),
        Recipe(listOf(BuildBlock.DIRT to 2, BuildBlock.SAND to 1), BuildBlock.BRICKS, 2),
        Recipe(listOf(BuildBlock.BRICKS to 2, BuildBlock.PLANKS to 1), BuildBlock.ROOF, 3),
        Recipe(listOf(BuildBlock.SAND to 1, BuildBlock.STONE to 1), BuildBlock.PLASTER, 2),
        Recipe(listOf(BuildBlock.PLANKS to 2, BuildBlock.GLASS to 1), BuildBlock.LAMP, 1),
    )

    /** Blocks found in the world: breaking them is the only other way to get blocks. */
    val NATURAL: Set<BuildBlock> = setOf(BuildBlock.WOOD, BuildBlock.STONE, BuildBlock.DIRT, BuildBlock.SAND, BuildBlock.LEAVES)
}
