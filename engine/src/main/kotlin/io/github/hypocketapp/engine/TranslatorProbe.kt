package io.github.hypocketapp.engine

import android.content.Context
import java.io.File
import java.util.concurrent.TimeUnit
import kotlin.concurrent.thread

/**
 * Proof of concept for the CPU translator: runs our own x86-64 Linux test
 * program (engine/guest-tests/probe) through Box64 and runs the same workload
 * natively, so we can check that the result is correct and compare speed and
 * memory. It uses no game files.
 *
 * Blocking: call it from a background thread.
 */
class TranslatorProbe(context: Context) {
    private val appContext = context.applicationContext

    private val box64 = File(appContext.applicationInfo.nativeLibraryDir, "libbox64.so")

    fun run(timeoutSeconds: Long = 120): ProbeReport {
        val native = NativeBridge.nativeRunWorkload()
        val nativeChecksum = native[0].toULong()
        val nativeMillis = native[1]

        if (!box64.exists()) {
            return ProbeReport(
                status = ProbeStatus.TRANSLATOR_NOT_AVAILABLE,
                nativeChecksum = nativeChecksum,
                nativeWorkloadMillis = nativeMillis,
            )
        }

        val guest = extractGuest()
        val process = ProcessBuilder(box64.absolutePath, guest.absolutePath)
            .directory(appContext.filesDir)
            .redirectErrorStream(true)
            .apply {
                environment()["HOME"] = appContext.filesDir.absolutePath
                environment()["TMPDIR"] = appContext.cacheDir.absolutePath
                environment()["BOX64_LOG"] = "1"
            }

        val startNanos = System.nanoTime()
        val started = process.start()
        val output = StringBuilder()
        val reader = thread(name = "box64-probe-output") {
            started.inputStream.bufferedReader().useLines { lines ->
                lines.forEach { line ->
                    synchronized(output) {
                        if (output.length < MAX_OUTPUT_CHARS) output.appendLine(line)
                    }
                }
            }
        }
        val finished = started.waitFor(timeoutSeconds, TimeUnit.SECONDS)
        if (!finished) started.destroyForcibly()
        reader.join(TimeUnit.SECONDS.toMillis(5))
        val totalMillis = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - startNanos)

        val text = synchronized(output) { output.toString() }
        val guestChecksum = Regex("""probe: checksum (\d+)""").find(text)?.groupValues?.get(1)?.toULongOrNull()
        val status = when {
            !finished -> ProbeStatus.TIMED_OUT
            started.exitValue() != 0 || !text.contains("probe: ok") -> ProbeStatus.FAILED
            guestChecksum != nativeChecksum -> ProbeStatus.WRONG_RESULT
            else -> ProbeStatus.PASSED
        }

        return ProbeReport(
            status = status,
            nativeChecksum = nativeChecksum,
            nativeWorkloadMillis = nativeMillis,
            translatedChecksum = guestChecksum,
            translatedWorkloadMillis = Regex("""probe: workload (\d+) ms""").find(text)
                ?.groupValues?.get(1)?.toLongOrNull(),
            totalMillis = totalMillis,
            peakMemoryKb = Regex("""VmHWM:\s+(\d+) kB""").find(text)?.groupValues?.get(1)?.toLongOrNull(),
            exitCode = if (finished) started.exitValue() else null,
            output = text,
        )
    }

    /** Copies the probe from the APK assets to private storage. */
    private fun extractGuest(): File {
        val target = File(appContext.filesDir, "guest-tests/$GUEST_NAME")
        target.parentFile?.mkdirs()
        appContext.assets.open("guest-tests/$GUEST_NAME").use { input ->
            target.outputStream().use { input.copyTo(it) }
        }
        return target
    }

    private companion object {
        const val GUEST_NAME = "probe-x86_64"
        const val MAX_OUTPUT_CHARS = 64 * 1024
    }
}

enum class ProbeStatus {
    PASSED,
    WRONG_RESULT,
    FAILED,
    TIMED_OUT,

    /** Box64 is not packaged for this device's ABI (e.g. x86_64 emulators). */
    TRANSLATOR_NOT_AVAILABLE,
}

data class ProbeReport(
    val status: ProbeStatus,
    val nativeChecksum: ULong,
    val nativeWorkloadMillis: Long,
    val translatedChecksum: ULong? = null,
    val translatedWorkloadMillis: Long? = null,
    /** Includes starting Box64 and the guest. */
    val totalMillis: Long? = null,
    /** Peak memory of the Box64 process, as reported by the guest. */
    val peakMemoryKb: Long? = null,
    val exitCode: Int? = null,
    val output: String = "",
)
