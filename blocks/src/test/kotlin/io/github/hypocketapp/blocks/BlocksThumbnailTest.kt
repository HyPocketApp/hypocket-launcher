package io.github.hypocketapp.blocks

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class BlocksThumbnailTest {
    private fun file(w: Int, h: Int, rgba: List<Int>): ByteArray =
        ByteBuffer.allocate(12 + rgba.size).order(ByteOrder.LITTLE_ENDIAN).apply {
            putInt(0x54425048)
            putInt(w)
            putInt(h)
            rgba.forEach { put(it.toByte()) }
        }.array()

    @Test
    fun rowsComeBottomUpAndBecomeArgb() {
        // Bottom row red, top row blue, as OpenGL reads them.
        val bytes = file(1, 2, listOf(255, 0, 0, 255, 0, 0, 255, 255))
        val picture = checkNotNull(BlocksThumbnail.decode(bytes))
        assertEquals(1, picture.width)
        assertEquals(2, picture.height)
        assertEquals(0xFF0000FF.toInt(), picture.pixels[0]) // top: blue
        assertEquals(0xFFFF0000.toInt(), picture.pixels[1]) // bottom: red
    }

    @Test
    fun badFilesAreRefused() {
        assertNull(BlocksThumbnail.decode(ByteArray(5)))
        assertNull(BlocksThumbnail.decode(file(2, 2, List(8) { 0 }))) // too short
        assertNull(BlocksThumbnail.decode(file(0, 2, emptyList())))
        val wrongMagic = file(1, 1, List(4) { 0 }).also { it[0] = 0 }
        assertNull(BlocksThumbnail.decode(wrongMagic))
    }
}

class BlocksCompletionTest {
    private fun header(version: Int, flags: Int, done: Float): ByteArray =
        java.nio.ByteBuffer.allocate(64).order(java.nio.ByteOrder.LITTLE_ENDIAN).apply {
            putInt(0, 0x57425048)
            putInt(4, version)
            putInt(56, flags)
            putFloat(60, done)
        }.array()

    @org.junit.Test
    fun completionTimeComesFromVersion4Headers() {
        org.junit.Assert.assertEquals(754, BlocksWorlds.completedSeconds(header(4, 3, 754.6f)))
        org.junit.Assert.assertNull(BlocksWorlds.completedSeconds(header(4, 1, 754f))) // beacons placed, not all lit
        org.junit.Assert.assertNull(BlocksWorlds.completedSeconds(header(3, 3, 754f))) // older saves have no goal
        org.junit.Assert.assertNull(BlocksWorlds.completedSeconds(ByteArray(10)))
    }
}
