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
