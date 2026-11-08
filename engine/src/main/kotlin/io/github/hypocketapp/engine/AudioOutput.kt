package io.github.hypocketapp.engine

import java.io.File

/**
 * Plays the sound a guest program writes into [file], a shared ring of
 * samples (layout in audio_output.cpp), through AAudio.
 */
class AudioOutput private constructor(private var handle: Long, val file: File) : AutoCloseable {

    /** What happened so far, for the test report. */
    val info: String get() = if (handle != 0L) NativeBridge.nativeAudioInfo(handle) else "closed"

    override fun close() {
        if (handle != 0L) NativeBridge.nativeAudioDestroy(handle)
        handle = 0
        file.delete()
    }

    companion object {
        /** Returns null when no audio stream could be opened. */
        fun create(file: File): AudioOutput? {
            val handle = NativeBridge.nativeAudioCreate(file.absolutePath)
            return if (handle == 0L) null else AudioOutput(handle, file)
        }
    }
}
