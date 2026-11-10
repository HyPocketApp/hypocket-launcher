package io.github.hypocketapp.blocks

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.nio.file.Files
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test

class BlocksModsTest {
    private lateinit var dir: File
    private lateinit var mods: BlocksMods

    @Before
    fun setUp() {
        dir = Files.createTempDirectory("blocks-mods").toFile()
        mods = BlocksMods(BlocksWorld("w1", "World", 8, 1, 0L, File(dir, "w1.hpw")))
    }

    @After
    fun tearDown() {
        dir.deleteRecursively()
    }

    private fun zip(vararg files: Pair<String, String>): ByteArrayInputStream {
        val bytes = ByteArrayOutputStream()
        ZipOutputStream(bytes).use { out ->
            for ((name, text) in files) {
                out.putNextEntry(ZipEntry(name))
                out.write(text.toByteArray())
                out.closeEntry()
            }
        }
        return ByteArrayInputStream(bytes.toByteArray())
    }

    @Test
    fun aModInOneFolderKeepsTheFoldersName() {
        val name = mods.add(zip("glow/things.txt" to "[block glow]", "glow/init.lua" to "-- hi"), "download.zip")
        assertEquals("glow", name)
        assertEquals(listOf("glow"), mods.list())
        assertTrue(File(mods.directory, "glow/things.txt").exists())
    }

    @Test
    fun aModWithItsFilesAtTheTopTakesTheFilesName() {
        val name = mods.add(zip("things.txt" to "[item cheese]"), "my cheese mod!.zip")
        assertEquals("my_cheese_mod", name)
        assertTrue(File(mods.directory, "my_cheese_mod/things.txt").exists())
    }

    @Test
    fun filesPointingOutsideTheModAreRefused() {
        try {
            mods.add(zip("../../escaped.txt" to "no"), "bad.zip")
            fail("an entry outside the mod was accepted")
        } catch (expected: IOException) {
        }
        assertFalse(File(dir, "escaped.txt").exists())
        assertFalse(File(dir.parentFile, "escaped.txt").exists())
        assertEquals(emptyList<String>(), mods.list())
    }

    @Test
    fun removingDeletesTheModOnly() {
        mods.add(zip("a/things.txt" to "x"), "a.zip")
        mods.add(zip("b/things.txt" to "y"), "b.zip")
        mods.remove("a")
        mods.remove("../w1.hpw")
        assertEquals(listOf("b"), mods.list())
    }
}
