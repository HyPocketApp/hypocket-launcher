package io.github.hypocketapp.engine

import android.view.Surface

/**
 * Shows the VirGL server's frames on [Surface] without copying them through
 * the CPU: the server renders into GPU buffers, and the app draws them onto
 * the surface with OpenGL ES (direct_display.cpp).
 */
class DirectDisplay private constructor(private var handle: Long, internal val socketName: String) : AutoCloseable {

    /** Frames that reached the screen so far. */
    val shownFrames: Int get() = if (handle != 0L) NativeBridge.nativeDisplayShownFrames(handle) else 0

    /** What happened so far, for the test report. */
    val info: String get() = if (handle != 0L) NativeBridge.nativeDisplayInfo(handle) else "closed"

    fun resize(width: Int, height: Int) {
        if (handle != 0L) NativeBridge.nativeDisplayResize(handle, width, height)
    }

    override fun close() {
        if (handle != 0L) NativeBridge.nativeDisplayDestroy(handle)
        handle = 0
    }

    companion object {
        /** Returns null when direct display could not be set up. */
        fun create(surface: Surface): DirectDisplay? {
            val name = arrayOfNulls<String>(1)
            val handle = NativeBridge.nativeDisplayCreate(surface, name)
            return if (handle == 0L) null else DirectDisplay(handle, name[0]!!)
        }
    }
}
