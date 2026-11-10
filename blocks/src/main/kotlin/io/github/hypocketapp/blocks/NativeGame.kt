package io.github.hypocketapp.blocks

import android.view.Surface

/**
 * The game built natively for the phone (libblocks.so, from the same
 * blocks/game/blocks.c as the emulated program): no Box64 or VirGL in the
 * way. It talks to the app through the same shared input file and
 * environment as the emulated build; frames go straight to a [Surface].
 *
 * The game keeps its state in globals, so it runs once per process: the
 * game screen has a process of its own (see AndroidManifest.xml) that ends
 * after a native game.
 */
object NativeGame {
    private val loaded = runCatching { System.loadLibrary("blocks") }.isSuccess

    /** The library is in this APK for this phone's CPU. */
    val isAvailable: Boolean get() = loaded

    /** Runs the game until it exits and returns its exit code. Blocking; it waits for a [setSurface]. */
    @JvmStatic external fun run(args: Array<String>, environment: Array<String>, logPath: String): Int

    /** The window to draw into, or null while there is none (the app is in the background). */
    @JvmStatic external fun setSurface(surface: Surface?)

    /** A finger on the game's own screens: [action] 0 down, 1 move, 2 up; [x] and [y] from 0 to 1 across the view. */
    @JvmStatic external fun touch(action: Int, id: Int, x: Float, y: Float)

    /** A key for the game's own screens (BLOCKS_KEY_* in blocks/game/platform.h). */
    @JvmStatic external fun key(key: Int)

    /** BLOCKS_KEY_MENU: open the menu, or go back. */
    const val KEY_MENU = 1

    /** Asks the game to stop waiting for a window; it quits when it sees the shared file's request. */
    @JvmStatic external fun stop()
}
