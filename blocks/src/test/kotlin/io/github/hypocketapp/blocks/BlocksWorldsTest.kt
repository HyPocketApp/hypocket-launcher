package io.github.hypocketapp.blocks

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import kotlin.random.Random

class BlocksWorldsTest {
    @get:Rule val folder = TemporaryFolder()

    @Test
    fun createdWorldIsListedWithItsSettings() {
        val worlds = BlocksWorlds(folder.root)
        val world = worlds.create("  Base do Heitor  ", 16, Random(1))
        assertEquals("Base do Heitor", world.name)
        assertEquals(16, world.chunks)
        assertTrue(world.seed in 0 until 1_000_000)
        assertFalse(world.saveFile.exists())
        assertEquals(listOf(world), worlds.list())
        assertEquals(world, worlds.get(world.id))
    }

    @Test
    fun mostRecentlyPlayedComesFirst() {
        val worlds = BlocksWorlds(folder.root)
        val older = worlds.create("A", 8)
        val newer = worlds.create("B", 8)
        older.saveFile.writeBytes(ByteArray(64))
        older.saveFile.setLastModified(System.currentTimeMillis() + 60_000)
        assertEquals(listOf(older.id, newer.id), worlds.list().map { it.id })
    }

    @Test
    fun deleteRemovesTheSaveToo() {
        val worlds = BlocksWorlds(folder.root)
        val world = worlds.create("A", 24)
        world.saveFile.writeBytes(ByteArray(64))
        val unreadable = java.io.File(folder.root, "${world.id}.hpw.unreadable").apply { writeBytes(ByteArray(64)) }
        worlds.delete(world.id)
        assertTrue(worlds.list().isEmpty())
        assertFalse(world.saveFile.exists())
        assertFalse(unreadable.exists())
    }

    @Test
    fun idsOutsideTheFolderAreRefused() {
        val worlds = BlocksWorlds(folder.root)
        assertNull(worlds.get("../secret"))
        worlds.delete("../secret") // must not throw or touch anything
    }

    @Test(expected = IllegalArgumentException::class)
    fun unsupportedSizeIsRefused() {
        BlocksWorlds(folder.root).create("A", 12)
    }

    @Test
    fun longAndEmptyNamesAreFixed() {
        val worlds = BlocksWorlds(folder.root)
        assertEquals(BlocksWorlds.MAX_NAME, worlds.create("x".repeat(100), 8).name.length)
        assertEquals("World", worlds.create("   ", 8).name)
    }
}
