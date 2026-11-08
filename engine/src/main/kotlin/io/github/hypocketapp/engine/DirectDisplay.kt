package io.github.hypocketapp.engine

import android.os.Build
import android.view.Surface

/**
 * Shows the VirGL server's frames on [Surface] without copying them through
 * the CPU: the server renders into GPU buffers that the system compositor
 * shows directly (direct_display.cpp). Needs Android 10 or newer.
 */
class DirectDisplay private constructor(private var handle: Long, internal val socketName: String) : AutoCloseable {

    /** Frames that reached the screen so far. */
    val shownFrames: Int get() = if (handle != 0L) NativeBridge.nativeDisplayShownFrames(handle) else 0

    fun resize(width: Int, height: Int) {
        if (handle != 0L) NativeBridge.nativeDisplayResize(handle, width, height)
    }

    override fun close() {
        if (handle != 0L) NativeBridge.nativeDisplayDestroy(handle)
        handle = 0
    }

    companion object {
        val isSupported: Boolean get() = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q

        /** Returns null when direct display is not possible on this phone. */
        fun create(surface: Surface): DirectDisplay? {
            if (!isSupported) return null
            val name = arrayOfNulls<String>(1)
            val handle = NativeBridge.nativeDisplayCreate(surface, name)
            return if (handle == 0L) null else DirectDisplay(handle, name[0]!!)
        }
    }
}
