package io.github.hypocketapp.engine

import java.io.File

/** System-wide memory from /proc/meminfo, in MB. Missing fields are -1. */
data class MemorySnapshot(
    val totalMb: Long,
    val availableMb: Long,
    val freeMb: Long,
    val cachedMb: Long,
    val swapTotalMb: Long,
    val swapFreeMb: Long,
) {
    companion object {
        fun read(): MemorySnapshot {
            val fields = runCatching {
                File("/proc/meminfo").readLines().associate { line ->
                    val name = line.substringBefore(':')
                    val kb = line.substringAfter(':').trim().substringBefore(' ').toLongOrNull() ?: -1L
                    name to kb
                }
            }.getOrDefault(emptyMap())
            fun mb(name: String) = fields[name]?.takeIf { it >= 0 }?.div(1024) ?: -1L
            return MemorySnapshot(
                totalMb = mb("MemTotal"),
                availableMb = mb("MemAvailable"),
                freeMb = mb("MemFree"),
                cachedMb = mb("Cached"),
                swapTotalMb = mb("SwapTotal"),
                swapFreeMb = mb("SwapFree"),
            )
        }
    }
}

/** Resident memory of one of the app's own processes. */
data class ProcessMemory(val pid: Int, val name: String, val rssMb: Long)

object AppProcesses {
    /**
     * The app's own processes (the app, Box64 and the VirGL server). Android
     * only shows an app the processes of its own user in /proc, so this never
     * sees other apps.
     */
    fun list(): List<ProcessMemory> = File("/proc").listFiles().orEmpty()
        .mapNotNull { dir -> dir.name.toIntOrNull()?.let { pid -> read(pid, dir) } }

    private val selfExe: String? by lazy { runCatching { File("/proc/self/exe").canonicalPath }.getOrNull() }

    private fun read(pid: Int, dir: File): ProcessMemory? = runCatching {
        // Starting a program forks the app: for a moment the child is still a
        // copy of the app (named after the forking thread) and would count the
        // app's memory twice.
        if (pid != android.os.Process.myPid() && selfExe != null &&
            runCatching { File(dir, "exe").canonicalPath }.getOrNull() == selfExe
        ) {
            return@runCatching null
        }
        var name = "?"
        var rssKb = -1L
        File(dir, "status").forEachLine { line ->
            when {
                line.startsWith("Name:") -> name = line.substringAfter(':').trim()
                line.startsWith("VmRSS:") -> rssKb = line.substringAfter(':').trim().substringBefore(' ').toLong()
            }
        }
        // Kernel threads have no VmRSS.
        if (rssKb < 0) null else ProcessMemory(pid, name, rssKb / 1024)
    }.getOrNull()
}

/**
 * Samples memory in the background while a test runs, to find the lowest
 * free memory and the peak of each of the app's processes.
 */
class MemorySampler(private val intervalMillis: Long = 250) : AutoCloseable {
    private val start = MemorySnapshot.read()
    @Volatile private var running = true
    private var lowest = start
    private val peaks = mutableMapOf<String, Long>()
    private val thread = Thread({
        while (running) {
            sample()
            try {
                Thread.sleep(intervalMillis)
            } catch (e: InterruptedException) {
                break
            }
        }
    }, "hypocket-memory-sampler").apply {
        isDaemon = true
        start()
    }

    private fun sample() {
        val now = MemorySnapshot.read()
        synchronized(this) {
            if (now.availableMb in 0 until lowest.availableMb) lowest = now
            AppProcesses.list().forEach { p -> peaks[p.name] = maxOf(peaks[p.name] ?: 0, p.rssMb) }
        }
    }

    /** Stops sampling and returns the summary. */
    fun finish(): MemoryUsage {
        close()
        synchronized(this) {
            return MemoryUsage(start, lowest, peaks.toList().sortedByDescending { it.second })
        }
    }

    override fun close() {
        running = false
        thread.interrupt()
        thread.join(1000)
    }
}

data class MemoryUsage(
    val atStart: MemorySnapshot,
    /** The sample with the least available memory. */
    val lowest: MemorySnapshot,
    /** Peak resident memory of each of the app's processes, by name. */
    val processPeaksMb: List<Pair<String, Long>>,
) {
    fun toLines(): List<String> = buildList {
        add("Available at start: ${atStart.availableMb} MB of ${atStart.totalMb} MB, swap free ${atStart.swapFreeMb} of ${atStart.swapTotalMb} MB")
        add("Lowest available: ${lowest.availableMb} MB, swap free ${lowest.swapFreeMb} MB")
        processPeaksMb.forEach { (name, mb) -> add("Peak $name: $mb MB") }
    }
}
