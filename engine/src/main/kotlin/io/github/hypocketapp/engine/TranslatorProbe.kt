package io.github.hypocketapp.engine

import android.content.Context

/**
 * Proof of concept for the CPU translator: runs our own x86-64 Linux test
 * program (engine/guest-tests/probe) through Box64 and runs the same workload
 * natively, so we can check that the result is correct and compare speed and
 * memory. It uses no game files.
 *
 * Blocking: call it from a background thread.
 */
class TranslatorProbe(context: Context) {
    private val runtime = GuestRuntime(context)

    fun run(timeoutSeconds: Long = 120): ProbeReport {
        val native = NativeBridge.nativeRunWorkload()
        val nativeChecksum = native[0].toULong()
        val nativeMillis = native[1]

        if (!runtime.isAvailable) {
            return ProbeReport(
                status = ProbeStatus.TRANSLATOR_NOT_AVAILABLE,
                nativeChecksum = nativeChecksum,
                nativeWorkloadMillis = nativeMillis,
            )
        }

        val result = runtime.runAsset("probe-x86_64", timeoutSeconds = timeoutSeconds)
        val text = result.output
        val guestChecksum = Regex("""probe: checksum (\d+)""").find(text)?.groupValues?.get(1)?.toULongOrNull()
        val status = when {
            !result.finished -> ProbeStatus.TIMED_OUT
            result.blockedSyscall -> ProbeStatus.BLOCKED_SYSCALL
            result.exitCode != 0 || !text.contains("probe: ok") -> ProbeStatus.FAILED
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
            totalMillis = result.millis,
            peakMemoryKb = Regex("""VmHWM:\s+(\d+) kB""").find(text)?.groupValues?.get(1)?.toLongOrNull(),
            exitCode = result.exitCode,
            output = text,
        )
    }
}

enum class ProbeStatus {
    PASSED,
    WRONG_RESULT,
    FAILED,
    TIMED_OUT,

    /** Android blocked a system call the translator or its runtime made. */
    BLOCKED_SYSCALL,

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
