package io.github.hypocketapp.blocks

import io.github.hypocketapp.blocks.ui.BuildBlock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BlocksRecipesTest {
    /** Every block that can be had starting from what the world gives. */
    private fun reachable(): Set<BuildBlock> {
        val have = BlocksRecipes.NATURAL.toMutableSet()
        do {
            val more = BlocksRecipes.ALL.filter { r -> r.inputs.all { it.first in have } }.map { it.output }
        } while (have.addAll(more))
        return have
    }

    @Test
    fun lampAndGlassCanBeMadeFromScratch() {
        assertTrue(BuildBlock.LAMP in reachable())
        assertTrue(BuildBlock.GLASS in reachable())
    }

    @Test
    fun everyBlockCanBeHad() {
        assertEquals(BuildBlock.entries.toSet(), reachable())
    }

    @Test
    fun someBlocksCanOnlyBeMade() {
        val made = BlocksRecipes.ALL.map { it.output }.toSet()
        assertTrue((made - BlocksRecipes.NATURAL).isNotEmpty())
        assertTrue(made.none { it in BlocksRecipes.NATURAL })
    }

    @Test
    fun canMakeChecksEveryInput() {
        val lamp = BlocksRecipes.ALL.first { it.output == BuildBlock.LAMP }
        val counts = MutableList(16) { 0 }
        counts[BuildBlock.PLANKS.ordinal] = 2
        assertFalse(lamp.canMake(counts))
        counts[BuildBlock.GLASS.ordinal] = 1
        assertTrue(lamp.canMake(counts))
    }

    @Test(expected = IllegalArgumentException::class)
    fun aRecipeCannotMakeItsOwnInput() {
        Recipe(listOf(BuildBlock.WOOD to 1), BuildBlock.WOOD, 2)
    }
}
