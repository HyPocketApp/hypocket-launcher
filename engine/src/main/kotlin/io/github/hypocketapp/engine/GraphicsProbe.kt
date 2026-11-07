package io.github.hypocketapp.engine

import android.content.Context

/** The graphics test programs in engine/guest-tests. */
enum class GraphicsTest(
    internal val asset: String,
    internal val tag: String,
    internal val env: Map<String, String> = emptyMap(),
) {
    /** OpenGL ES 2 triangle (engine/guest-tests/gl-probe). */
    GLES("gl-probe-x86_64", "gl-probe"),

    /**
     * Desktop OpenGL core profile, which the game needs (4.1 or newer)
     * (engine/guest-tests/gl-core-probe).
     */
    DESKTOP_CORE("gl-core-probe-x86_64", "gl-core-probe"),

    /**
     * The same, with Mesa told to report OpenGL 4.1 even if the phone lacks
     * some 4.x features (fp64, 16384-pixel textures, ...). Many games ask for
     * 4.1 but do not use them.
     */
    DESKTOP_CORE_FORCED(
        "gl-core-probe-x86_64",
        "gl-core-probe",
        mapOf("MESA_GL_VERSION_OVERRIDE" to "4.1", "MESA_GLSL_VERSION_OVERRIDE" to "410"),
    ),
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

    fun run(test: GraphicsTest, timeoutSeconds: Long = 120): GraphicsReport {
        if (!runtime.isAvailable || !VirglServer.binary(runtime).exists()) {
            return GraphicsReport(test, GraphicsStatus.NOT_AVAILABLE)
        }

        val (server, failureLog) = VirglServer.start(runtime)
        if (server == null) {
            return GraphicsReport(test = test, status = GraphicsStatus.SERVER_FAILED, serverOutput = failureLog)
        }

        server.use {
            val result = runtime.runAsset(
                test.asset,
                extraEnv = server.guestEnvironment + test.env,
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
            server.awaitExit(3)
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
                serverOutput = server.log,
            )
        }
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
