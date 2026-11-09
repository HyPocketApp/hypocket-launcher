package io.github.hypocketapp.engine

import java.io.File
import java.util.concurrent.TimeUnit
import kotlin.concurrent.thread

/**
 * The VirGL render server (libvirgl_server.so), which runs the guest's GL
 * commands on the phone's own OpenGL ES driver. Each instance serves one
 * client and then exits.
 */
internal class VirglServer private constructor(
    private val process: Process,
    private val reader: Thread,
    private val output: StringBuilder,
    val socket: File,
) : AutoCloseable {

    val log: String get() = synchronized(output) { output.toString() }

    /** Environment the guest needs to reach this server through Mesa's virgl driver. */
    val guestEnvironment: Map<String, String>
        get() = mapOf(
            "VTEST_SOCKET_NAME" to socket.absolutePath,
            "GALLIUM_DRIVER" to "virpipe",
            "LIBGL_ALWAYS_SOFTWARE" to "1",
            "EGL_PLATFORM" to "surfaceless",
        )

    /** Waits for the server to exit after its client disconnected. */
    fun awaitExit(seconds: Long) {
        process.waitFor(seconds, TimeUnit.SECONDS)
    }

    override fun close() {
        if (process.isAlive) process.destroyForcibly()
        reader.join(2000)
        socket.delete()
    }

    companion object {
        private const val MAX_OUTPUT_CHARS = 32 * 1024

        fun binary(runtime: GuestRuntime) = File(runtime.nativeLibDir, "libvirgl_server.so")

        /**
         * Starts the server, or returns null and the server's log if it did
         * not come up. [onLine] gets each line of its output as it comes.
         */
        fun start(
            runtime: GuestRuntime,
            extraEnv: Map<String, String> = emptyMap(),
            onLine: (String) -> Unit = {},
        ): Pair<VirglServer?, String> {
            val socket = File(runtime.cacheDir, "virgl.sock")
            socket.delete()
            val process = ProcessBuilder(
                binary(runtime).absolutePath,
                // Serve one client, then exit.
                "--no-loop-or-fork",
                "--use-egl-surfaceless",
                "--use-gles",
                "--socket-path", socket.absolutePath,
            )
                .redirectErrorStream(true)
                .apply {
                    environment()["LD_LIBRARY_PATH"] = runtime.nativeLibDir.absolutePath
                    // Log the features the server finds in the phone's driver.
                    environment()["VREND_DEBUG"] = "feat"
                    environment()["VIRGL_LOG_LEVEL"] = "debug"
                    environment().putAll(extraEnv)
                }
                .start()
            val output = StringBuilder()
            val reader = thread(name = "virgl-server-output") {
                process.inputStream.bufferedReader().useLines { lines ->
                    lines.forEach { line ->
                        synchronized(output) {
                            if (output.length < MAX_OUTPUT_CHARS) output.appendLine(line)
                        }
                        onLine(line)
                    }
                }
            }
            val server = VirglServer(process, reader, output, socket)
            repeat(50) {
                if (socket.exists()) return server to ""
                if (!process.isAlive) {
                    server.close()
                    return null to server.log
                }
                Thread.sleep(100)
            }
            if (socket.exists()) return server to ""
            server.close()
            return null to server.log
        }
    }
}
