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
class SharedFrame private constructor(private val buffer: MappedByteBuffer, private val capacity: Long) {
    private var lastFrame = 0

    /** Size of the newest frame; it can change while the guest runs. */
    val width: Int get() = buffer.getInt(4)
    val height: Int get() = buffer.getInt(8)

    /** Number of frames the guest has finished so far. */
    val frameCounter: Int get() = buffer.getInt(12)

    /** True while the guest is still drawing. */
    val isRunning: Boolean get() = buffer.getInt(16) != 0

    /** True when there is a frame that has not been copied yet. */
    val hasNewFrame: Boolean get() = frameCounter != lastFrame && width > 0 && height > 0

    /** Copies the newest frame into [bitmap], which must be [width] x [height]. */
    fun copyFrame(bitmap: Bitmap): Boolean {
        val w = width
        val h = height
        val bytes = w.toLong() * h * 4
        if (w <= 0 || h <= 0 || HEADER + bytes > capacity || bitmap.width != w || bitmap.height != h) return false
        lastFrame = frameCounter
        val pixels = buffer.duplicate()
        pixels.position(HEADER)
        pixels.limit(HEADER + bytes.toInt())
        bitmap.copyPixelsFromBuffer(pixels.slice())
        return true
    }

    companion object {
        const val HEADER = 64
        private const val MAGIC = 0x42465048

        /** Maps the file once the guest has created it and written its header, or returns null. */
        fun open(file: File): SharedFrame? {
            if (!file.exists() || file.length() < HEADER) return null
            RandomAccessFile(file, "r").use { raf ->
                val length = raf.length()
                val map = raf.channel.map(FileChannel.MapMode.READ_ONLY, 0, length)
                map.order(ByteOrder.LITTLE_ENDIAN)
                if (map.getInt(0) != MAGIC) return null
                return SharedFrame(map, length)
            }
        }
    }
}
