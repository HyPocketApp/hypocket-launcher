package io.github.hypocketapp.engine

/** JNI declarations. Only [Engine] may call these. */
internal object NativeBridge {
    init {
        System.loadLibrary("hypocket_engine")
    }

    external fun nativeVersion(): String

    external fun nativeAbi(): String
}
