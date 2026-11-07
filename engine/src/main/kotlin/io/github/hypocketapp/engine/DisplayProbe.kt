package io.github.hypocketapp.engine

import android.content.Context
import java.io.File

/**
 * Runs the display probe (engine/guest-tests/gl-display-probe): an animated
 * scene drawn by an x86-64 program through Box64 and VirGL with a desktop
 * OpenGL 4.1 core context, read back into [frameFile] for the app to show.
 *
 * Blocking: call it from a background thread, and show [frameFile] meanwhile
 * with [SharedFrame].
 */
class DisplayProbe(context: Context) {
    private val runtime = GuestRuntime(context)

    val frameFile: File = File(runtime.cacheDir, "hypocket-frame.bin")

    fun run(seconds: Int = 15): DisplayReport {
        frameFile.delete()
        if (!runtime.isAvailable || !VirglServer.binary(runtime).exists()) {
            return DisplayReport(DisplayStatus.NOT_AVAILABLE)
        }
        val (server, failureLog) = VirglServer.start(runtime)
        if (server == null) return DisplayReport(DisplayStatus.SERVER_FAILED, serverOutput = failureLog)

        server.use {
            val result = runtime.runAsset(
                "gl-display-probe-x86_64",
                extraEnv = server.guestEnvironment + mapOf(
                    "HYPOCKET_FRAME_PATH" to frameFile.absolutePath,
                    "MESA_GL_VERSION_OVERRIDE" to "4.1",
                    "MESA_GLSL_VERSION_OVERRIDE" to "410",
                ),
                args = listOf(seconds.toString()),
                timeoutSeconds = seconds + 60L,
            )
            server.awaitExit(3)
            val text = result.output
            val fps = Regex("""gl-display-probe: \d+ frames in \d+ ms, ([\d.]+) fps""").find(text)
            val perFrame = Regex("""per frame: draw ([\d.]+) ms, read back ([\d.]+) ms""").find(text)
            return DisplayReport(
                status = when {
                    !result.finished -> DisplayStatus.TIMED_OUT
                    result.blockedSyscall -> DisplayStatus.BLOCKED_SYSCALL
                    result.exitCode != 0 || !text.contains("gl-display-probe: ok") -> DisplayStatus.FAILED
                    else -> DisplayStatus.PASSED
                },
                renderer = Regex("""gl-display-probe: GL_RENDERER (.*)""").find(text)?.groupValues?.get(1),
                guestFps = fps?.groupValues?.get(1)?.toDoubleOrNull(),
                drawMillis = perFrame?.groupValues?.get(1)?.toDoubleOrNull(),
                readBackMillis = perFrame?.groupValues?.get(2)?.toDoubleOrNull(),
                exitCode = result.exitCode,
                output = text,
                serverOutput = server.log,
            )
        }
    }
}

enum class DisplayStatus { PASSED, FAILED, TIMED_OUT, BLOCKED_SYSCALL, SERVER_FAILED, NOT_AVAILABLE }

data class DisplayReport(
    val status: DisplayStatus,
    val renderer: String? = null,
    /** Frames per second the guest drew and read back. */
    val guestFps: Double? = null,
    val drawMillis: Double? = null,
    val readBackMillis: Double? = null,
    val exitCode: Int? = null,
    val output: String = "",
    val serverOutput: String = "",
)
