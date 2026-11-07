package io.github.hypocketapp.engine

import java.io.File
import android.content.Context

/**
 * Proof of concept for the graphics path: runs our own x86-64 OpenGL ES test
 * program (engine/guest-tests/gl-probe) through Box64. Its EGL/GL calls go to
 * Mesa's virgl driver in the guest, which sends them to a VirGL server running
 * on Android, which draws with the phone's own GPU driver. The program draws a
 * triangle off screen and checks the colors it reads back. No game files.
 *
 * Blocking: call it from a background thread.
 */
class GraphicsProbe(context: Context) {
    private val runtime = GuestRuntime(context)
    private val server = File(runtime.nativeLibDir, "libvirgl_server.so")

    fun run(timeoutSeconds: Long = 120): GraphicsReport {
        if (!runtime.isAvailable || !server.exists()) {
            return GraphicsReport(GraphicsStatus.NOT_AVAILABLE)
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
                    status = GraphicsStatus.SERVER_FAILED,
                    serverOutput = synchronized(serverOutput) { serverOutput.toString() },
                )
            }

            val result = runtime.runAsset(
                "gl-probe-x86_64",
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
                result.exitCode != 0 || !text.contains("gl-probe: ok") -> GraphicsStatus.FAILED
                else -> GraphicsStatus.PASSED
            }
            serverProcess.waitFor(3, java.util.concurrent.TimeUnit.SECONDS)
            return GraphicsReport(
                status = status,
                renderer = Regex("""gl-probe: GL_RENDERER (.*)""").find(text)?.groupValues?.get(1),
                version = Regex("""gl-probe: GL_VERSION (.*)""").find(text)?.groupValues?.get(1),
                frameMillis = Regex("""gl-probe: \d+ frames in (\d+) ms""").find(text)
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
    FAILED,
    TIMED_OUT,
    BLOCKED_SYSCALL,

    /** The VirGL server did not start (for example, no usable GLES driver). */
    SERVER_FAILED,

    /** Box64 or the VirGL server is not packaged for this device's ABI. */
    NOT_AVAILABLE,
}

data class GraphicsReport(
    val status: GraphicsStatus,
    val renderer: String? = null,
    val version: String? = null,
    /** Time to draw the probe's 120 frames. */
    val frameMillis: Long? = null,
    val totalMillis: Long? = null,
    val exitCode: Int? = null,
    val output: String = "",
    val serverOutput: String = "",
)
