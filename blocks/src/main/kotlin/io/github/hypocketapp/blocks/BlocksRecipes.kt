package io.github.hypocketapp.blocks

import io.github.hypocketapp.blocks.ui.BuildBlock
import io.github.hypocketapp.blocks.ui.BuildBlock.Kind

/**
 * One way to make blocks or items from others: no crafting grid, just what goes
 * in and what comes out, and what it needs without using it up: a [tool]
 * carried and a workstation ([station]) within reach.
 */
data class Recipe(
    /** The blocks or items used up, each with how many (one or two kinds). */
    val inputs: List<Pair<BuildBlock, Int>>,
    val output: BuildBlock,
    val count: Int,
    /** A workstation that must stand near the player (a kiln, a stove or a campfire). */
    val station: BuildBlock? = null,
    /** A tool the player must carry. */
    val tool: BuildBlock? = null,
) {
    init {
        require(inputs.size in 1..2 && inputs.all { it.second in 1..MAX_AMOUNT } && count in 1..MAX_AMOUNT)
        require(inputs.none { it.first == output })
        require(station == null || station.stationBit != 0)
        require(tool == null || tool.kind == Kind.TOOL)
    }

    /**
     * Whether [counts] (blocks and items carried, by [BuildBlock] index) are
     * enough to make it once, with the tool, where [stations] (the
     * workstations near the player, [BlocksStats.survivalFlags]) say.
     */
    fun canMake(counts: List<Int>, stations: Int): Boolean =
        inputs.all { (block, n) -> counts.getOrElse(block.ordinal) { 0 } >= n } && hasTool(counts) && hasStation(stations)

    fun hasTool(counts: List<Int>): Boolean = tool == null || counts.getOrElse(tool.ordinal) { 0 } > 0

    fun hasStation(stations: Int): Boolean = station == null || stations and station.stationBit != 0

    companion object {
        /** What one byte of the shared file can hold. */
        const val MAX_AMOUNT = 64
    }
}

/**
 * Every recipe, in the order the inventory lists them: the one table to change
 * to rebalance the game. The game only checks and applies what it is sent
 * (see BlocksSession.craft and craft() in blocks.c), including the tool and
 * the workstation.
 */
object BlocksRecipes {
    val ALL: List<Recipe> = listOf(
        // Improvised tools, from what lies on the ground.
        Recipe(listOf(BuildBlock.STICK to 1, BuildBlock.FLINT to 1), BuildBlock.KNIFE, 1),
        Recipe(listOf(BuildBlock.STICK to 2, BuildBlock.FLINT to 2), BuildBlock.PICK, 1),
        Recipe(listOf(BuildBlock.STICK to 2, BuildBlock.FLINT to 3), BuildBlock.AXE, 1),
        // Wood: a knife strips a log into a plank; a saw cuts two, and cuts a plank into sticks.
        Recipe(listOf(BuildBlock.WOOD to 1), BuildBlock.PLANKS, 1, tool = BuildBlock.KNIFE),
        Recipe(listOf(BuildBlock.WOOD to 1), BuildBlock.PLANKS, 2, tool = BuildBlock.SAW),
        Recipe(listOf(BuildBlock.PLANKS to 1), BuildBlock.STICK, 8, tool = BuildBlock.SAW),
        // Workstations.
        Recipe(listOf(BuildBlock.STICK to 4, BuildBlock.STONE to 3), BuildBlock.CAMPFIRE, 1),
        Recipe(listOf(BuildBlock.STONE to 6, BuildBlock.DIRT to 2), BuildBlock.KILN, 1),
        Recipe(listOf(BuildBlock.STONE_BRICKS to 4, BuildBlock.BRICKS to 2), BuildBlock.STOVE, 1),
        // Building.
        Recipe(listOf(BuildBlock.STONE to 1), BuildBlock.STONE_BRICKS, 1, tool = BuildBlock.PICK),
        Recipe(listOf(BuildBlock.SAND to 1), BuildBlock.GLASS, 1, station = BuildBlock.KILN),
        Recipe(listOf(BuildBlock.DIRT to 2, BuildBlock.SAND to 1), BuildBlock.BRICKS, 2, station = BuildBlock.KILN),
        Recipe(listOf(BuildBlock.SAND to 1, BuildBlock.STONE to 1), BuildBlock.PLASTER, 2, station = BuildBlock.KILN),
        Recipe(listOf(BuildBlock.BRICKS to 2, BuildBlock.PLANKS to 1), BuildBlock.ROOF, 3),
        Recipe(listOf(BuildBlock.PLANKS to 2, BuildBlock.GLASS to 1), BuildBlock.LAMP, 1),
        // Food and clothes.
        Recipe(listOf(BuildBlock.RAW_MEAT to 1), BuildBlock.COOKED_MEAT, 1, station = BuildBlock.CAMPFIRE),
        Recipe(listOf(BuildBlock.RAW_MEAT to 1, BuildBlock.FAT to 1), BuildBlock.FRIED_MEAT, 2, station = BuildBlock.STOVE),
        Recipe(listOf(BuildBlock.HIDE to 3, BuildBlock.WOOL to 2), BuildBlock.COAT, 1, tool = BuildBlock.KNIFE),
    )

    /**
     * What the world gives without a recipe: blocks broken in it (shade stone
     * on the other side), things lying on the ground (sticks, flint, and the
     * knife and saw left in the ruins), stoves in the ruins, berries from
     * leaves and what hunting gives.
     */
    val NATURAL: Set<BuildBlock> = setOf(
        BuildBlock.WOOD, BuildBlock.STONE, BuildBlock.DIRT, BuildBlock.SAND, BuildBlock.LEAVES, BuildBlock.SHADE,
        BuildBlock.STICK, BuildBlock.FLINT, BuildBlock.KNIFE, BuildBlock.SAW, BuildBlock.STOVE, BuildBlock.BERRIES,
        BuildBlock.RAW_MEAT, BuildBlock.FAT, BuildBlock.HIDE, BuildBlock.WOOL,
    )
}
