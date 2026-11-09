package io.github.hypocketapp.engine

import org.junit.Assert.assertEquals
import org.junit.Test

class OutputKeeperTest {
    @Test
    fun keepsEverythingThatFits() {
        val keeper = OutputKeeper(headChars = 100, tailChars = 100)
        keeper.add("one")
        keeper.add("two")
        assertEquals("one\ntwo\n", keeper.toString())
    }

    @Test
    fun keepsTheStartAndTheEnd() {
        val keeper = OutputKeeper(headChars = 8, tailChars = 8)
        (1..10).forEach { keeper.add("l$it") }
        // Head: l1, l2 (6 chars; l3 would make 9). Tail: the last lines in 8 chars.
        assertEquals("l1\nl2\n... 6 lines left out ...\nl9\nl10\n", keeper.toString())
    }
}
