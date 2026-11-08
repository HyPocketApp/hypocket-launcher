package io.github.hypocketapp.engine

/** JNI declarations. Only [Engine] may call these. */
internal object NativeBridge {
    init {
        System.loadLibrary("hypocket_engine")
    }

    external fun nativeVersion(): String

    external fun nativeAbi(): String

    /** Returns [checksum, elapsed milliseconds]. */
    external fun nativeRunWorkload(): LongArray

    external fun nativeDisplayCreate(surface: android.view.Surface, socketNameOut: Array<String?>): Long

    external fun nativeDisplayResize(handle: Long, width: Int, height: Int)

    external fun nativeDisplayShownFrames(handle: Long): Int

    external fun nativeDisplayDestroy(handle: Long)

    external fun nativeDisplayInfo(handle: Long): String
}
