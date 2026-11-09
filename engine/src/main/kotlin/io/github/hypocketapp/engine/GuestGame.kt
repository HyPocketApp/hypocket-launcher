package io.github.hypocketapp.engine

import android.content.Context
import java.io.File

/**
 * Runs an x86-64 Linux game through the whole HyPocket path: Box64 for the
 * CPU, Mesa's virgl driver and HyPocket's VirGL server for desktop OpenGL 4.1
 * on the phone's GPU, [DirectDisplay] for the frames, an audio ring played
 * with AAudio, and precise fences between the guest's Mesa and the server.
 *
 * This is the engine's interface for games: HyPocket Blocks (the `:blocks`
 * module) uses only this, and so will the real game. Blocking: call [run]
 * from a background thread.
 */
class GuestGame(context: Context) {
    private val runtime = GuestRuntime(context)

    /** Private storage for files the game and the app share. */
    val cacheDir: File get() = runtime.cacheDir

    /** Box64, the guest runtime and the VirGL server are in this APK. */
    val isAvailable: Boolean get() = runtime.isAvailable && VirglServer.binary(runtime).exists()

    class Launch(
        /** A program in the APK's assets, under guest-tests/. */
        val asset: String,
        val args: List<String> = emptyList(),
        /** More environment for the game. */
        val env: Map<String, String> = emptyMap(),
        /** More environment for the VirGL server (its options). */
        val serverEnv: Map<String, String> = emptyMap(),
        /** Where frames go; without it the game must read them back itself. */
        val display: DirectDisplay? = null,
        /** The audio ring's file; the game gets it as HYPOCKET_AUDIO_PATH. Null: silent. */
        val audioFile: File? = null,
        /** Precise fences (HYPOCKET_FENCES); off only to compare with upstream's. */
        val preciseFences: Boolean = true,
        val timeoutSeconds: Long,
        /** Called once the game's process started, to be able to stop it. */
        val onStart: (Process) -> Unit = {},
        /**
         * Where the game's and the server's output also go as they come, so
         * it is kept on the phone even if the run is cut off. The caller
         * finishes it with its own report.
         */
        val log: RunLog? = null,
    )

    class Result(
        val status: DisplayStatus,
        val exitCode: Int? = null,
        val seconds: Double = 0.0,
        /** The game's own output (stdout and stderr). */
        val output: String = "",
        val serverOutput: String = "",
        val memory: MemoryUsage? = null,
        /** What the audio output saw, or why there was none. */
        val audioInfo: String = "",
        /** The direct display's statistics, if one was used. */
        val directInfo: String = "",
    )

    /**
     * Runs the game until it exits. The status is PASSED when it exited with
     * code 0; the caller can check the output for more.
     */
    fun run(launch: Launch): Result {
        if (!isAvailable) return Result(DisplayStatus.NOT_AVAILABLE)
        // A fresh page each run: the server numbers command buffers from 1.
        val fenceFile = File(runtime.cacheDir, "hypocket-fences.bin")
        fenceFile.writeBytes(ByteArray(FENCE_PAGE_SIZE))
        val fenceEnv = mapOf("HYPOCKET_FENCE_PATH" to fenceFile.absolutePath)
        val display = launch.display
        val serverEnv = fenceEnv +
            (if (display != null) mapOf("HYPOCKET_DISPLAY_SOCKET" to display.socketName) else emptyMap()) +
            launch.serverEnv
        val log = launch.log
        val (server, failureLog) = VirglServer.start(runtime, serverEnv) { log?.line("[server] $it") }
        if (server == null) {
            log?.line("[launcher] the VirGL server did not start")
            return Result(DisplayStatus.SERVER_FAILED, serverOutput = failureLog)
        }
        val audio = launch.audioFile?.let { AudioOutput.create(it) }
        val gameEnv = buildMap {
            putAll(server.guestEnvironment)
            put("MESA_GL_VERSION_OVERRIDE", "4.1")
            put("MESA_GLSL_VERSION_OVERRIDE", "410")
            putAll(fenceEnv)
            // Precise fences between our Mesa and our VirGL server (see
            // engine/third_party/mesa-guest).
            put("HYPOCKET_FENCES", if (launch.preciseFences) "1" else "0")
            if (display != null) put("HYPOCKET_PRESENT", "1")
            if (audio != null) put("HYPOCKET_AUDIO_PATH", audio.file.absolutePath)
            putAll(launch.env)
        }
        try {
            MemorySampler().use { sampler ->
                server.use {
                    val result = runtime.runAsset(
                        launch.asset,
                        extraEnv = gameEnv,
                        args = launch.args,
                        timeoutSeconds = launch.timeoutSeconds,
                        onStart = launch.onStart,
                        onLine = { log?.line(it) },
                    )
                    log?.line("[launcher] game exited: code ${result.exitCode ?: "-"}, finished ${result.finished}")
                    val memory = sampler.finish()
                    server.awaitExit(3)
                    return Result(
                        status = when {
                            // The server found no usable OpenGL ES driver (an emulator, for one).
                            server.log.contains("couldn't find an OpenGL ES implementation") -> DisplayStatus.SERVER_FAILED
                            !result.finished -> DisplayStatus.TIMED_OUT
                            result.blockedSyscall -> DisplayStatus.BLOCKED_SYSCALL
                            result.exitCode != 0 -> DisplayStatus.FAILED
                            else -> DisplayStatus.PASSED
                        },
                        exitCode = result.exitCode,
                        seconds = result.millis / 1000.0,
                        output = result.output,
                        serverOutput = server.log,
                        memory = memory,
                        audioInfo = audio?.info ?: "no audio stream",
                        directInfo = display?.info.orEmpty(),
                    )
                }
            }
        } finally {
            audio?.close()
            fenceFile.delete()
        }
    }

    private companion object {
        const val FENCE_PAGE_SIZE = 4096
    }
}
