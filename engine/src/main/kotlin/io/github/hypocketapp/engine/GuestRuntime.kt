package io.github.hypocketapp.engine

import android.content.Context
import android.os.Build
import java.io.File
import java.nio.file.Files
import java.util.concurrent.TimeUnit
import kotlin.concurrent.thread

/**
 * Runs x86-64 Linux programs through Box64. Box64 is the Linux (glibc) build,
 * so it is started through glibc's dynamic loader, shipped in the APK with a
 * minimal glibc runtime and Mesa (virgl driver) for graphics.
 *
 * Blocking: call it from a background thread.
 */
internal class GuestRuntime(context: Context) {
    private val appContext = context.applicationContext
    val nativeLibDir = File(appContext.applicationInfo.nativeLibraryDir)
    private val box64 = File(nativeLibDir, "libbox64.so")
    private val loader = File(nativeLibDir, "libglibc_ld.so")

    val isAvailable: Boolean get() = box64.exists() && loader.exists()

    val filesDir: File get() = appContext.filesDir
    val cacheDir: File get() = appContext.cacheDir

    /** Runs a test program from the APK assets (guest-tests/<name>). */
    fun runAsset(
        name: String,
        extraEnv: Map<String, String> = emptyMap(),
        args: List<String> = emptyList(),
        timeoutSeconds: Long,
        onStart: (Process) -> Unit = {},
        onLine: (String) -> Unit = {},
    ): RunResult {
        val guest = extractAsset(name)
        val libDir = prepareLibDir()
        val builder = ProcessBuilder(
            listOf(
                loader.absolutePath,
                "--library-path", libDir.absolutePath,
                box64.absolutePath,
                guest.absolutePath,
            ) + args,
        )
            .directory(appContext.filesDir)
            .redirectErrorStream(true)
            .apply {
                environment()["HOME"] = appContext.filesDir.absolutePath
                environment()["TMPDIR"] = appContext.cacheDir.absolutePath
                environment()["BOX64_LOG"] = "1"
                environment().putAll(HostCpu.box64Environment)
                environment().putAll(extraEnv)
            }
        return run(builder, timeoutSeconds, onStart, onLine)
    }

    /**
     * Creates a directory where the guest runtime has its real file names, as
     * symbolic links to the renamed copies in the native library dir.
     */
    private fun prepareLibDir(): File {
        val dir = File(appContext.filesDir, "glibc")
        dir.mkdirs()
        GUEST_RUNTIME.forEach { (name, apkName) ->
            val link = File(dir, name).toPath()
            Files.deleteIfExists(link)
            val target = File(nativeLibDir, apkName)
            if (target.exists()) Files.createSymbolicLink(link, target.toPath())
        }
        return dir
    }

    /** Copies a test program from the APK assets to private storage. */
    private fun extractAsset(name: String): File {
        val target = File(appContext.filesDir, "guest-tests/$name")
        target.parentFile?.mkdirs()
        appContext.assets.open("guest-tests/$name").use { input ->
            target.outputStream().use { input.copyTo(it) }
        }
        // Box64 refuses files without the executable bit. Android still does
        // not let the kernel run it directly: Box64 only reads it, and falls
        // back to copying the code into memory when mapping it is denied.
        check(target.setExecutable(true, true)) { "Could not mark ${target.name} as executable" }
        return target
    }

    /**
     * Box64 reads the CPU name, core count and frequency at startup. ARM
     * kernels do not list a CPU name in /proc/cpuinfo, so Box64 then runs
     * `lscpu` through a shell, which does not exist on Android and slows every
     * start. Box64 skips all of that when BOX64_SYSINFO_CACHED is set
     * (src/os/sysinfo.c), so the app reads the values once and passes them.
     */
    private object HostCpu {
        val box64Environment: Map<String, String> by lazy {
            buildMap {
                put("BOX64_SYSINFO_CACHED", "1")
                put("BOX64_SYSINFO_NCPU", Runtime.getRuntime().availableProcessors().toString())
                put("BOX64_SYSINFO_CPUNAME", cpuName())
                maxFrequencyHz()?.let { put("BOX64_SYSINFO_FREQUENCY", it.toString()) }
            }
        }

        private fun cpuName(): String {
            val soc = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) Build.SOC_MODEL else ""
            return soc.takeIf { it.isNotBlank() && it != Build.UNKNOWN } ?: "ARM64 CPU"
        }

        /** Fastest core's maximum frequency, from cpufreq (kHz there). */
        private fun maxFrequencyHz(): Long? = File("/sys/devices/system/cpu").listFiles().orEmpty()
            .filter { it.name.matches(Regex("cpu\\d+")) }
            .mapNotNull { runCatching { File(it, "cpufreq/cpuinfo_max_freq").readText().trim().toLong() }.getOrNull() }
            .maxOrNull()
            ?.times(1000)
    }

    data class RunResult(val finished: Boolean, val exitCode: Int?, val millis: Long, val output: String) {
        val blockedSyscall: Boolean get() = exitCode == EXIT_SIGSYS
    }

    companion object {
        /** 128 + SIGSYS: Android's seccomp filter killed the process. */
        const val EXIT_SIGSYS = 128 + 31
        private const val MAX_OUTPUT_CHARS = 64 * 1024

        /** Must match GUEST_RUNTIME in engine/build.gradle.kts. */
        val GUEST_RUNTIME = mapOf(
            "ld-linux-aarch64.so.1" to "libglibc_ld.so",
            "libc.so.6" to "libglibc_c.so",
            "libm.so.6" to "libglibc_m.so",
            "libresolv.so.2" to "libglibc_resolv.so",
            "libdl.so.2" to "libglibc_dl.so",
            "libpthread.so.0" to "libglibc_pthread.so",
            "librt.so.1" to "libglibc_rt.so",
            "libutil.so.1" to "libglibc_util.so",
            "libgcc_s.so.1" to "libglibc_gcc_s.so",
            "libstdc++.so.6" to "libglibc_stdcxx.so",
            "libEGL.so.1" to "libmesa_egl.so",
            "libGLESv2.so.2" to "libmesa_glesv2.so",
            "libgallium-26.2.6.so" to "libmesa_gallium.so",
            "libdrm.so.2" to "libmesa_drm.so",
            "libexpat.so.1" to "libmesa_expat.so",
            "libz.so.1" to "libmesa_z.so",
        )

        /**
         * Starts a process and collects its output, killing it after the
         * timeout. [onLine] gets each line as it comes, on the reader thread.
         */
        fun run(
            builder: ProcessBuilder,
            timeoutSeconds: Long,
            onStart: (Process) -> Unit = {},
            onLine: (String) -> Unit = {},
        ): RunResult {
            val startNanos = System.nanoTime()
            val process = builder.start()
            onStart(process)
            val output = StringBuilder()
            val reader = thread(name = "guest-output") {
                process.inputStream.bufferedReader().useLines { lines ->
                    lines.forEach { line ->
                        synchronized(output) {
                            if (output.length < MAX_OUTPUT_CHARS) output.appendLine(line)
                        }
                        onLine(line)
                    }
                }
            }
            val finished = process.waitFor(timeoutSeconds, TimeUnit.SECONDS)
            if (!finished) process.destroyForcibly()
            reader.join(TimeUnit.SECONDS.toMillis(5))
            val millis = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - startNanos)
            return RunResult(
                finished = finished,
                exitCode = if (finished) process.exitValue() else null,
                millis = millis,
                output = synchronized(output) { output.toString() },
            )
        }
    }
}
