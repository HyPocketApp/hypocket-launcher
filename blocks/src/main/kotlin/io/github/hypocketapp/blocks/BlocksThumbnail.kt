package io.github.hypocketapp.blocks

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * A world's small picture, written by the game next to the save
 * (save_thumbnail in blocks.c): "HPBT", width and height as 32-bit little
 * endian numbers, then RGBA rows from the bottom up.
 */
class BlocksThumbnail(val width: Int, val height: Int, /** ARGB, top row first, ready for a Bitmap. */ val pixels: IntArray) {
    companion object {
        private const val MAGIC = 0x54425048
        private const val MAX_SIDE = 512

        /** The picture in [bytes], or null if they do not hold a valid one. */
        fun decode(bytes: ByteArray): BlocksThumbnail? {
            if (bytes.size < 12) return null
            val header = ByteBuffer.wrap(bytes, 0, 12).order(ByteOrder.LITTLE_ENDIAN)
            if (header.int != MAGIC) return null
            val w = header.int
            val h = header.int
            if (w !in 1..MAX_SIDE || h !in 1..MAX_SIDE || bytes.size < 12 + w * h * 4) return null
            val pixels = IntArray(w * h)
            for (y in 0 until h) {
                val src = 12 + (h - 1 - y) * w * 4 // bottom-up rows
                for (x in 0 until w) {
                    val i = src + x * 4
                    val r = bytes[i].toInt() and 0xFF
                    val g = bytes[i + 1].toInt() and 0xFF
                    val b = bytes[i + 2].toInt() and 0xFF
                    pixels[y * w + x] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
                }
            }
            return BlocksThumbnail(w, h, pixels)
        }
    }
}
