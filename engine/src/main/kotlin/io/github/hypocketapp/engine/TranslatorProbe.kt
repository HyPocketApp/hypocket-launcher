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
 * Box64 is the Linux (glibc) build, so it is started through glibc's dynamic
 * loader, shipped in the APK together with a minimal glibc runtime.
 *
 * Blocking: call it from a background thread.
 */
class TranslatorProbe(context: Context) {
    private val appContext = context.applicationContext

    private val nativeLibDir = File(appContext.applicationInfo.nativeLibraryDir)
    private val box64 = File(nativeLibDir, "libbox64.so")
    private val loader = File(nativeLibDir, "libglibc_ld.so")

    fun run(timeoutSeconds: Long = 120): ProbeReport {
        val native = NativeBridge.nativeRunWorkload()
        val nativeChecksum = native[0].toULong()
        val nativeMillis = native[1]

        if (!box64.exists() || !loader.exists()) {
            return ProbeReport(
                status = ProbeStatus.TRANSLATOR_NOT_AVAILABLE,
                nativeChecksum = nativeChecksum,
                nativeWorkloadMillis = nativeMillis,
            )
        }

        val guest = extractGuest()
        val glibcDir = prepareGlibcDir()
        val process = ProcessBuilder(
            loader.absolutePath,
            "--library-path", glibcDir.absolutePath,
            box64.absolutePath,
            guest.absolutePath,
        )
            .directory(appContext.filesDir)
            .redirectErrorStream(true)
            .apply {
                environment()["HOME"] = appContext.filesDir.absolutePath
                environment()["TMPDIR"] = appContext.cacheDir.absolutePath
                environment()["BOX64_LOG"] = "1"
                // glibc registers rseq at startup; Android's seccomp filter
                // may not allow it, so turn it off.
                environment()["GLIBC_TUNABLES"] = "glibc.pthread.rseq=0"
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

    /**
     * Creates a directory where the glibc runtime has its real file names,
     * as symbolic links to the renamed copies in the native library dir.
     */
    private fun prepareGlibcDir(): File {
        val dir = File(appContext.filesDir, "glibc")
        dir.mkdirs()
        GLIBC_RUNTIME.forEach { (name, apkName) ->
            val link = File(dir, name).toPath()
            java.nio.file.Files.deleteIfExists(link)
            java.nio.file.Files.createSymbolicLink(link, File(nativeLibDir, apkName).toPath())
        }
        return dir
    }

    /** Copies the probe from the APK assets to private storage. */
    private fun extractGuest(): File {
        val target = File(appContext.filesDir, "guest-tests/$GUEST_NAME")
        target.parentFile?.mkdirs()
        appContext.assets.open("guest-tests/$GUEST_NAME").use { input ->
            target.outputStream().use { input.copyTo(it) }
        }
        // Box64 refuses files without the executable bit. Android still does
        // not let the kernel run it directly: Box64 only reads it, and falls
        // back to copying the code into memory when mapping it is denied.
        check(target.setExecutable(true, true)) { "Could not mark ${target.name} as executable" }
        return target
    }

    private companion object {
        const val GUEST_NAME = "probe-x86_64"

        /** Must match GLIBC_RUNTIME in engine/build.gradle.kts. */
        val GLIBC_RUNTIME = mapOf(
            "ld-linux-aarch64.so.1" to "libglibc_ld.so",
            "libc.so.6" to "libglibc_c.so",
            "libm.so.6" to "libglibc_m.so",
            "libresolv.so.2" to "libglibc_resolv.so",
            "libdl.so.2" to "libglibc_dl.so",
            "libpthread.so.0" to "libglibc_pthread.so",
            "librt.so.1" to "libglibc_rt.so",
            "libutil.so.1" to "libglibc_util.so",
            "libgcc_s.so.1" to "libglibc_gcc_s.so",
        )
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
