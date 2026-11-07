package io.github.hypocketapp.engine

import android.graphics.Bitmap
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteOrder
import java.nio.MappedByteBuffer
import java.nio.channels.FileChannel

/**
 * Reads the frames the display probe writes into a shared file (layout in
 * engine/guest-tests/gl-display-probe/gl_display_probe.c). This is a temporary
 * display path that costs a copy per frame.
 */
class SharedFrame private constructor(private val buffer: MappedByteBuffer, val width: Int, val height: Int) {
    private var lastFrame = 0

    /** Number of frames the guest has finished so far. */
    val frameCounter: Int get() = buffer.getInt(12)

    /** True while the guest is still drawing. */
    val isRunning: Boolean get() = buffer.getInt(16) != 0

    /** Copies the newest frame into [bitmap] if there is one; returns whether it did. */
    fun copyNewFrame(bitmap: Bitmap): Boolean {
        val frame = frameCounter
        if (frame == lastFrame) return false
        lastFrame = frame
        val pixels = buffer.duplicate()
        pixels.position(HEADER)
        pixels.limit(HEADER + width * height * 4)
        bitmap.copyPixelsFromBuffer(pixels.slice())
        return true
    }

    companion object {
        const val HEADER = 64
        private const val MAGIC = 0x42465048

        /** Maps the file once the guest has created and filled its header, or returns null. */
        fun open(file: File): SharedFrame? {
            if (!file.exists() || file.length() < HEADER) return null
            RandomAccessFile(file, "r").use { raf ->
                val map = raf.channel.map(FileChannel.MapMode.READ_ONLY, 0, raf.length())
                map.order(ByteOrder.LITTLE_ENDIAN)
                if (map.getInt(0) != MAGIC) return null
                val width = map.getInt(4)
                val height = map.getInt(8)
                if (width <= 0 || height <= 0 || raf.length() < HEADER + width.toLong() * height * 4) return null
                return SharedFrame(map, width, height)
            }
        }
    }
}
