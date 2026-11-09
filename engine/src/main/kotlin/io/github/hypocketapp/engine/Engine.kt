package io.github.hypocketapp.engine

import android.os.Build

/**
 * Kotlin entry point to the native engine.
 *
 * The engine does not know about accounts or screens. Starting the game is
 * done by the core module, which only does it for a verified account.
 */
object Engine {
    /** ABI the game client needs to be translated to. */
    const val TARGET_ABI = "arm64-v8a"

    val version: String by lazy { NativeBridge.nativeVersion() }

    val abi: String by lazy { NativeBridge.nativeAbi() }

    /** True when this device has the CPU architecture HyPocket targets. */
    fun isSupportedDevice(): Boolean = TARGET_ABI in Build.SUPPORTED_ABIS
}
