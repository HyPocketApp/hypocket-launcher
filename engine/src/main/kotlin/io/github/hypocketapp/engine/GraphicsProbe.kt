package io.github.hypocketapp.engine

import java.io.File
import android.content.Context

/** The graphics test programs in engine/guest-tests. */
enum class GraphicsTest(internal val asset: String, internal val tag: String) {
    /** OpenGL ES 2 triangle (engine/guest-tests/gl-probe). */
    GLES("gl-probe-x86_64", "gl-probe"),

    /**
     * Desktop OpenGL core profile, which the game needs (4.1 or newer)
     * (engine/guest-tests/gl-core-probe).
     */
    DESKTOP_CORE("gl-core-probe-x86_64", "gl-core-probe"),
}

/**
 * Proof of concept for the graphics path: runs one of our own x86-64 OpenGL
 * test programs through Box64. Its EGL/GL calls go to
 * Mesa's virgl driver in the guest, which sends them to a VirGL server running
 * on Android, which draws with the phone's own GPU driver. The program draws a
 * triangle off screen and checks the colors it reads back. No game files.
 *
 * Blocking: call it from a background thread.
 */
class GraphicsProbe(context: Context) {
    private val runtime = GuestRuntime(context)
    private val server = File(runtime.nativeLibDir, "libvirgl_server.so")

    fun run(test: GraphicsTest, timeoutSeconds: Long = 120): GraphicsReport {
        if (!runtime.isAvailable || !server.exists()) {
            return GraphicsReport(test, GraphicsStatus.NOT_AVAILABLE)
        }

        val socket = File(runtime.cacheDir, "virgl.sock")
        socket.delete()
        val serverProcess = ProcessBuilder(
            server.absolutePath,
            // Serve one client, then exit.
            "--no-loop-or-fork",
            "--use-egl-surfaceless",
            "--use-gles",
            "--socket-path", socket.absolutePath,
        )
            .redirectErrorStream(true)
            .apply { environment()["LD_LIBRARY_PATH"] = runtime.nativeLibDir.absolutePath }
            .start()
        val serverOutput = StringBuilder()
        val serverReader = kotlin.concurrent.thread(name = "virgl-server-output") {
            serverProcess.inputStream.bufferedReader().useLines { lines ->
                lines.forEach { line -> synchronized(serverOutput) { serverOutput.appendLine(line) } }
            }
        }

        try {
            if (!waitForSocket(socket, serverProcess)) {
                serverProcess.destroyForcibly()
                serverReader.join(2000)
                return GraphicsReport(
                    test = test,
                    status = GraphicsStatus.SERVER_FAILED,
                    serverOutput = synchronized(serverOutput) { serverOutput.toString() },
                )
            }

            val result = runtime.runAsset(
                test.asset,
                extraEnv = mapOf(
                    "VTEST_SOCKET_NAME" to socket.absolutePath,
                    "GALLIUM_DRIVER" to "virpipe",
                    "LIBGL_ALWAYS_SOFTWARE" to "1",
                    "EGL_PLATFORM" to "surfaceless",
                ),
                timeoutSeconds = timeoutSeconds,
            )
            val text = result.output
            val status = when {
                !result.finished -> GraphicsStatus.TIMED_OUT
                result.blockedSyscall -> GraphicsStatus.BLOCKED_SYSCALL
                result.exitCode == 2 -> GraphicsStatus.WRONG_RESULT
                result.exitCode == 3 -> GraphicsStatus.BELOW_REQUIRED_VERSION
                result.exitCode != 0 || !text.contains("${test.tag}: ok") -> GraphicsStatus.FAILED
                else -> GraphicsStatus.PASSED
            }
            serverProcess.waitFor(3, java.util.concurrent.TimeUnit.SECONDS)
            val tag = Regex.escape(test.tag)
            return GraphicsReport(
                test = test,
                status = status,
                renderer = Regex("""$tag: GL_RENDERER (.*)""").find(text)?.groupValues?.get(1),
                version = Regex("""$tag: GL_VERSION (.*)""").find(text)?.groupValues?.get(1),
                highestCoreVersion = Regex("""$tag: highest core (\d+\.\d+)""").find(text)?.groupValues?.get(1),
                frameMillis = Regex("""$tag: \d+ frames in (\d+) ms""").find(text)
                    ?.groupValues?.get(1)?.toLongOrNull(),
                totalMillis = result.millis,
                exitCode = result.exitCode,
                output = text,
                serverOutput = synchronized(serverOutput) { serverOutput.toString() },
            )
        } finally {
            if (serverProcess.isAlive) serverProcess.destroyForcibly()
            serverReader.join(2000)
            socket.delete()
        }
    }

    private fun waitForSocket(socket: File, server: Process): Boolean {
        repeat(50) {
            if (socket.exists()) return true
            if (!server.isAlive) return false
            Thread.sleep(100)
        }
        return socket.exists()
    }
}

enum class GraphicsStatus {
    PASSED,
    WRONG_RESULT,

    /** Drew correctly, but the highest desktop GL core version is below 4.1. */
    BELOW_REQUIRED_VERSION,
    FAILED,
    TIMED_OUT,
    BLOCKED_SYSCALL,

    /** The VirGL server did not start (for example, no usable GLES driver). */
    SERVER_FAILED,

    /** Box64 or the VirGL server is not packaged for this device's ABI. */
    NOT_AVAILABLE,
}

data class GraphicsReport(
    val test: GraphicsTest,
    val status: GraphicsStatus,
    val renderer: String? = null,
    val version: String? = null,
    /** Highest desktop OpenGL core profile version (DESKTOP_CORE only). */
    val highestCoreVersion: String? = null,
    /** Time to draw the probe's 120 frames. */
    val frameMillis: Long? = null,
    val totalMillis: Long? = null,
    val exitCode: Int? = null,
    val output: String = "",
    val serverOutput: String = "",
)
