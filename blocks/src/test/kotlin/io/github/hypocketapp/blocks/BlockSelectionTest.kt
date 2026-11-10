package io.github.hypocketapp.blocks

import io.github.hypocketapp.blocks.ui.BlockSelection
import io.github.hypocketapp.blocks.ui.BuildBlock
import io.github.hypocketapp.blocks.ui.Inventory
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class BlockSelectionTest {
    private val sent = mutableListOf<Int>()
    private val arranged = mutableListOf<List<Int>>()

    private fun selection(initial: List<Int> = emptyList(), creative: Boolean = false) =
        BlockSelection(initial, creative, send = { sent += it }, onArranged = { arranged += it })

    @Test
    fun defaultsDependOnTheMode() {
        assertEquals(BuildBlock.entries.take(9).map { it.ordinal }, selection(creative = true).slots.toList())
        val survival = selection().slots.toList()
        assertEquals(BuildBlock.DIRT.ordinal, survival[0])
        assertEquals(9, survival.toSet().size)
    }

    @Test
    fun invalidSavedArrangementFallsBackToTheDefault() {
        assertEquals(selection().slots.toList(), selection(listOf(1, 1, 1)).slots.toList())
        assertEquals(selection().slots.toList(), selection(listOf(0, 1, 2, 3, 4, 5, 6, 7, 99)).slots.toList())
    }

    @Test
    fun selectWrapsAndTellsTheGameTheBlock() {
        val s = selection(creative = true)
        s.select(-1)
        assertEquals(8, s.index.intValue)
        assertEquals(s.slots[8], sent.last())
        s.next()
        assertEquals(0, s.index.intValue)
    }

    @Test
    fun assignSwapsWithTheBlocksOtherPlace() {
        val s = selection(creative = true) // places hold blocks 0..8
        s.select(2)
        s.assign(5)
        assertEquals(5, s.slots[2])
        assertEquals(2, s.slots[5])
        assertEquals(5, sent.last())
        assertEquals(s.slots.toList(), arranged.last())
        s.assign(BuildBlock.LEAVES.ordinal) // not in the bar yet
        assertEquals(BuildBlock.LEAVES.ordinal, s.slots[2])
        assertEquals(9, s.slots.toSet().size)
    }

    @Test
    fun collectedBlocksTakeEmptyPlacesInSurvival() {
        val s = selection(listOf(9, 7, 8, 10, 11, 0, 1, 3, 6))
        val counts = MutableList(16) { 0 }
        counts[9] = 5 // dirt, already in the bar
        counts[BuildBlock.ROOF.ordinal] = 2 // not in the bar
        s.fill(counts)
        assertEquals(9, s.slots[0])
        assertEquals(BuildBlock.ROOF.ordinal, s.slots[1]) // first place whose block ran out
        assertTrue(arranged.isNotEmpty())
    }

    @Test
    fun creativeNeverRearranges() {
        val s = selection(creative = true)
        val before = s.slots.toList()
        s.fill(List(16) { 1 })
        assertEquals(before, s.slots.toList())
    }

    @Test
    fun inventoryCursorWalksTheGridAndTheRecipes() {
        val inventory = Inventory(recipes = 3)
        inventory.move(-1, 0)
        assertEquals(Inventory.BLOCKS - 1, inventory.cursor.intValue) // wraps around the grid
        inventory.cursor.intValue = Inventory.COLUMNS - 1
        inventory.move(1, 0) // right from the last column: the first recipe
        assertEquals(Inventory.BLOCKS, inventory.cursor.intValue)
        inventory.move(0, -1) // up from the first recipe: the last one
        assertEquals(Inventory.BLOCKS + 2, inventory.cursor.intValue)
        inventory.move(-1, 0) // back to the grid's last column
        assertEquals(2 * Inventory.COLUMNS - 1, inventory.cursor.intValue)
        val made = mutableListOf<Int>()
        inventory.cursor.intValue = Inventory.BLOCKS + 1
        inventory.confirm(selection(), craft = { made += it })
        assertEquals(listOf(1), made)
        inventory.show()
        assertTrue(inventory.open.value)
    }

    @Test
    fun withoutRecipesTheCursorStaysInTheGrid() {
        val inventory = Inventory()
        inventory.cursor.intValue = Inventory.COLUMNS - 1
        inventory.move(1, 0)
        assertEquals(Inventory.COLUMNS, inventory.cursor.intValue)
    }
}
