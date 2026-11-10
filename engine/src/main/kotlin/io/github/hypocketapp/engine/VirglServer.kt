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
    private val output: OutputKeeper,
    val socket: File,
) : AutoCloseable {

    val log: String get() = output.toString()

    /**
     * The path of the shared memory [name] the server made (see [start]'s
     * `memfds`), or null. The app and the game open it like a file.
     */
    fun memfdPath(name: String): String? {
        val prefix = "$MEMFD_LOG$name "
        // The server prints the lines before its socket exists, but they can
        // still be on their way through the output pipe.
        repeat(20) {
            log.lineSequence().firstOrNull { it.startsWith(prefix) }?.let { line ->
                return line.removePrefix(prefix).trim().takeIf { it.startsWith("/") }
            }
            // The server could not make it (its reason is in the log).
            if (log.lineSequence().any { it.startsWith("$MEMFD_FAILED_LOG$name ") }) return null
            if (!process.isAlive) return null
            Thread.sleep(25)
        }
        return null
    }

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
        /** Output kept for the report: its first and last halves. */
        private const val MAX_OUTPUT_CHARS = 32 * 1024
        private const val MEMFD_LOG = "hypocket-memfd: "
        private const val MEMFD_FAILED_LOG = "hypocket-memfd-failed: "

        fun binary(runtime: GuestRuntime) = File(runtime.nativeLibDir, "libvirgl_server.so")

        /**
         * Starts the server, or returns null and the server's log if it did not
         * come up. [memfds] (name to bytes) are pieces of shared memory the
         * server makes for the app and the game: see [memfdPath]. They need
         * memfd_create, which apps may only call from Android 11 on. [onLine]
         * gets each line of the server's output as it comes.
         */
        fun start(
            runtime: GuestRuntime,
            extraEnv: Map<String, String> = emptyMap(),
            memfds: Map<String, Int> = emptyMap(),
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
                    if (memfds.isNotEmpty()) {
                        environment()["HYPOCKET_MEMFDS"] = memfds.entries.joinToString(",") { "${it.key}:${it.value}" }
                    }
                }
                .start()
            val output = OutputKeeper(headChars = MAX_OUTPUT_CHARS / 2, tailChars = MAX_OUTPUT_CHARS / 2)
            val reader = thread(name = "virgl-server-output") {
                process.inputStream.bufferedReader().useLines { lines ->
                    lines.forEach { line ->
                        output.add(line)
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
