package io.github.hypocketapp.engine

import android.content.Context
import java.io.File

/** The on-screen test programs in engine/guest-tests. */
enum class DisplayTest(internal val asset: String, internal val tag: String) {
    /** An animated scene at 640x360 (gl-display-probe). */
    SCENE("gl-display-probe-x86_64", "gl-display-probe"),

    /**
     * A block terrain at 50%, 75% and 100% of the screen's resolution, with
     * three loads each, timing the GPU per frame (gl-perf-probe).
     */
    PERFORMANCE("gl-perf-probe-x86_64", "gl-perf-probe"),
}

/**
 * Runs an on-screen test: an x86-64 program draws through Box64 and VirGL
 * with a desktop OpenGL 4.1 core context, and reads its frames back into
 * [frameFile] for the app to show.
 *
 * Blocking: call it from a background thread, and show [frameFile] meanwhile
 * with [SharedFrame].
 */
class DisplayProbe(context: Context) {
    private val runtime = GuestRuntime(context)

    val frameFile: File = File(runtime.cacheDir, "hypocket-frame.bin")

    /**
     * [screenWidth] and [screenHeight] are the phone's screen in pixels; the
     * performance test derives its resolutions from them.
     */
    fun run(test: DisplayTest, screenWidth: Int, screenHeight: Int, seconds: Int = 15): DisplayReport {
        frameFile.delete()
        if (!runtime.isAvailable || !VirglServer.binary(runtime).exists()) {
            return DisplayReport(test, DisplayStatus.NOT_AVAILABLE)
        }
        val (server, failureLog) = VirglServer.start(runtime)
        if (server == null) return DisplayReport(test, DisplayStatus.SERVER_FAILED, serverOutput = failureLog)

        MemorySampler().use { sampler -> server.use {
            val result = runtime.runAsset(
                test.asset,
                extraEnv = server.guestEnvironment + mapOf(
                    "HYPOCKET_FRAME_PATH" to frameFile.absolutePath,
                    "MESA_GL_VERSION_OVERRIDE" to "4.1",
                    "MESA_GLSL_VERSION_OVERRIDE" to "410",
                ),
                args = when (test) {
                    DisplayTest.SCENE -> listOf(seconds.toString())
                    DisplayTest.PERFORMANCE -> listOf(screenWidth.toString(), screenHeight.toString(), "3")
                },
                timeoutSeconds = if (test == DisplayTest.PERFORMANCE) 240L else seconds + 60L,
            )
            val memory = sampler.finish()
            server.awaitExit(3)
            val text = result.output
            val fps = Regex("""gl-display-probe: \d+ frames in \d+ ms, ([\d.]+) fps""").find(text)
            val perFrame = Regex("""per frame: draw ([\d.]+) ms, read back ([\d.]+) ms""").find(text)
            return DisplayReport(
                test = test,
                status = when {
                    !result.finished -> DisplayStatus.TIMED_OUT
                    result.blockedSyscall -> DisplayStatus.BLOCKED_SYSCALL
                    result.exitCode != 0 || !text.contains("${test.tag}: ok") -> DisplayStatus.FAILED
                    else -> DisplayStatus.PASSED
                },
                renderer = Regex("""${Regex.escape(test.tag)}: GL_RENDERER (.*)""").find(text)?.groupValues?.get(1),
                steps = text.lines().filter { it.startsWith("perf: ") }.map { it.removePrefix("perf: ") },
                guestFps = fps?.groupValues?.get(1)?.toDoubleOrNull(),
                drawMillis = perFrame?.groupValues?.get(1)?.toDoubleOrNull(),
                readBackMillis = perFrame?.groupValues?.get(2)?.toDoubleOrNull(),
                exitCode = result.exitCode,
                memory = memory,
                output = text,
                serverOutput = server.log,
            )
        } }
    }
}

enum class DisplayStatus { PASSED, FAILED, TIMED_OUT, BLOCKED_SYSCALL, SERVER_FAILED, NOT_AVAILABLE }

data class DisplayReport(
    val test: DisplayTest,
    val status: DisplayStatus,
    val renderer: String? = null,
    /** Frames per second the guest drew and read back. */
    val guestFps: Double? = null,
    val drawMillis: Double? = null,
    val readBackMillis: Double? = null,
    /** Performance test results, one line per resolution and load. */
    val steps: List<String> = emptyList(),
    val exitCode: Int? = null,
    /** Memory while the test ran. */
    val memory: MemoryUsage? = null,
    val output: String = "",
    val serverOutput: String = "",
)
