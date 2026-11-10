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
    /** Why the ring is not in memory although that was asked for, or null. */
    private val memoryError: String?,
) : AutoCloseable {

    val inMemory: Boolean get() = path != file.absolutePath

    /** What happened so far, for the test report. */
    val info: String
        get() = if (handle != 0L) {
            NativeBridge.nativeAudioInfo(handle) + when {
                inMemory -> ", ring in memory"
                memoryError != null -> ", ring in a file (not in memory: $memoryError)"
                else -> ", ring in a file"
            }
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
         * Puts the ring in the shared memory at [memoryPath] when given, else
         * (or if that fails) in [file]. [memoryError] says why there is no
         * memory path although one was asked for. Returns null and calls
         * [onError] when no ring or audio stream could be set up.
         */
        fun create(
            file: File,
            memoryPath: String? = null,
            memoryError: String? = null,
            onError: (String) -> Unit = {},
        ): AudioOutput? {
            var whyNotMemory = memoryError
            if (memoryPath != null) {
                val handle = NativeBridge.nativeAudioCreate(memoryPath, existing = true)
                if (handle != 0L) return AudioOutput(handle, file, memoryPath, memoryError = null)
                whyNotMemory = "$memoryPath: ${NativeBridge.nativeAudioLastError()}"
                onError("audio ring not in memory, $whyNotMemory")
            }
            val handle = NativeBridge.nativeAudioCreate(file.absolutePath, existing = false)
            if (handle == 0L) {
                onError("no audio: ${NativeBridge.nativeAudioLastError()}")
                return null
            }
            return AudioOutput(handle, file, file.absolutePath, whyNotMemory)
        }
    }
}
