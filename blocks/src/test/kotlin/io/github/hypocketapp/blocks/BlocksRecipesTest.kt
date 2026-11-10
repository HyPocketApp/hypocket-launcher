package io.github.hypocketapp.blocks

import io.github.hypocketapp.blocks.ui.BuildBlock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BlocksRecipesTest {
    /**
     * Every block and item that can be had starting from what the world gives:
     * a recipe counts once its inputs, its tool and its workstation can be had.
     */
    private fun reachable(): Set<BuildBlock> {
        val have = BlocksRecipes.NATURAL.toMutableSet()
        do {
            val more = BlocksRecipes.ALL.filter { r ->
                r.inputs.all { it.first in have } && (r.tool == null || r.tool in have) && (r.station == null || r.station in have)
            }.map { it.output }
        } while (have.addAll(more))
        return have
    }

    /** Counts with [carried] of each block or item named, the rest 0. */
    private fun counts(vararg carried: Pair<BuildBlock, Int>): List<Int> =
        MutableList(BuildBlock.entries.size) { 0 }.also { list -> carried.forEach { (b, n) -> list[b.ordinal] = n } }

    @Test
    fun lampAndGlassCanBeMadeFromScratch() {
        assertTrue(BuildBlock.LAMP in reachable())
        assertTrue(BuildBlock.GLASS in reachable())
    }

    @Test
    fun everyBlockAndItemCanBeHad() {
        assertEquals(BuildBlock.entries.toSet(), reachable())
    }

    @Test
    fun someThingsCanOnlyBeMade() {
        val made = BlocksRecipes.ALL.map { it.output }.toSet()
        assertTrue(BuildBlock.LAMP in made - BlocksRecipes.NATURAL)
        assertTrue(BuildBlock.COAT in made - BlocksRecipes.NATURAL)
    }

    @Test
    fun aKnifeAndAPickaxeCanBeMadeFromWhatLiesNearTheStart() {
        // Three sticks and three flints lie near the start (NEAR_START in blocks.c).
        val start = counts(BuildBlock.STICK to 3, BuildBlock.FLINT to 3)
        val knife = BlocksRecipes.ALL.first { it.output == BuildBlock.KNIFE }
        val pick = BlocksRecipes.ALL.first { it.output == BuildBlock.PICK }
        assertTrue(knife.canMake(start, 0))
        val afterKnife = counts(BuildBlock.STICK to 2, BuildBlock.FLINT to 2)
        assertTrue(pick.canMake(afterKnife, 0))
    }

    @Test
    fun canMakeChecksEveryInput() {
        val lamp = BlocksRecipes.ALL.first { it.output == BuildBlock.LAMP }
        assertFalse(lamp.canMake(counts(BuildBlock.PLANKS to 2), 0))
        assertTrue(lamp.canMake(counts(BuildBlock.PLANKS to 2, BuildBlock.GLASS to 1), 0))
    }

    @Test
    fun aToolIsNeededButNotUsedUp() {
        val strip = BlocksRecipes.ALL.first { it.output == BuildBlock.PLANKS && it.tool == BuildBlock.KNIFE }
        assertFalse(strip.canMake(counts(BuildBlock.WOOD to 1), 0))
        assertTrue(strip.canMake(counts(BuildBlock.WOOD to 1, BuildBlock.KNIFE to 1), 0))
        assertTrue(strip.inputs.none { it.first == BuildBlock.KNIFE })
    }

    @Test
    fun aSawGivesMoreThanAKnife() {
        val byKnife = BlocksRecipes.ALL.first { it.output == BuildBlock.PLANKS && it.tool == BuildBlock.KNIFE }
        val bySaw = BlocksRecipes.ALL.first { it.output == BuildBlock.PLANKS && it.tool == BuildBlock.SAW }
        assertTrue(bySaw.count > byKnife.count)
        val sticks = BlocksRecipes.ALL.first { it.output == BuildBlock.STICK }
        assertEquals(BuildBlock.SAW, sticks.tool)
        assertTrue(sticks.count >= 8)
    }

    @Test
    fun aWorkstationMustBeNear() {
        val glass = BlocksRecipes.ALL.first { it.output == BuildBlock.GLASS }
        val sand = counts(BuildBlock.SAND to 1)
        assertFalse(glass.canMake(sand, 0))
        assertFalse(glass.canMake(sand, BlocksStats.NEAR_CAMPFIRE))
        assertTrue(glass.canMake(sand, BlocksStats.NEAR_KILN or BlocksStats.NEAR_CAMPFIRE))
    }

    @Test
    fun onlyBlocksTakeTheHotbarAndFoodIsEaten() {
        assertTrue(BuildBlock.entries.filter { it.usable }.all { it.kind == BuildBlock.Kind.BLOCK || it.kind == BuildBlock.Kind.FOOD })
        assertEquals(
            setOf(BuildBlock.KILN, BuildBlock.STOVE, BuildBlock.CAMPFIRE),
            BuildBlock.entries.filter { it.stationBit != 0 }.toSet(),
        )
    }

    @Test(expected = IllegalArgumentException::class)
    fun aRecipeCannotMakeItsOwnInput() {
        Recipe(listOf(BuildBlock.WOOD to 1), BuildBlock.WOOD, 2)
    }

    @Test(expected = IllegalArgumentException::class)
    fun onlyToolsAreTools() {
        Recipe(listOf(BuildBlock.WOOD to 1), BuildBlock.PLANKS, 1, tool = BuildBlock.STICK)
    }
}
