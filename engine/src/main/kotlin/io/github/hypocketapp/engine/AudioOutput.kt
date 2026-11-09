package io.github.hypocketapp.engine

import java.io.File

/**
 * Plays the sound a guest program writes into a shared ring of samples
 * (layout in audio_output.cpp), through AAudio. The ring is [file], or the
 * shared memory at [path] when it was made with a memory path.
 */
class AudioOutput private constructor(
    private var handle: Long,
    val file: File,
    /** What the game opens: the file, or the shared memory. */
    val path: String,
) : AutoCloseable {

    val inMemory: Boolean get() = path != file.absolutePath

    /** What happened so far, for the test report. */
    val info: String
        get() = if (handle != 0L) {
            NativeBridge.nativeAudioInfo(handle) + if (inMemory) ", ring in memory" else ", ring in a file"
        } else {
            "closed"
        }

    override fun close() {
        if (handle != 0L) NativeBridge.nativeAudioDestroy(handle)
        handle = 0
        if (!inMemory) file.delete()
    }

    companion object {
        /** The ring's size in bytes; must match audio_output.cpp. */
        const val RING_BYTES = 64 + 8192 * 2 * 2

        /**
         * Returns null when no audio stream could be opened, or the ring could
         * not be set up at [memoryPath] (when given; [file] is then not used).
         */
        fun create(file: File, memoryPath: String? = null): AudioOutput? {
            val path = memoryPath ?: file.absolutePath
            val handle = NativeBridge.nativeAudioCreate(path)
            return if (handle == 0L) null else AudioOutput(handle, file, path)
        }
    }
}
