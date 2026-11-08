package io.github.hypocketapp.launcher.diagnostics

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import android.os.Debug
import android.os.PowerManager
import io.github.hypocketapp.engine.AppProcesses
import io.github.hypocketapp.engine.MemorySnapshot

/**
 * What the phone's memory looks like right now, to understand why so little
 * is free. Android does not let apps see other apps' memory, so this only
 * shows totals and HyPocket's own processes.
 */
object MemoryReport {
    fun build(context: Context): String = buildString {
        val am = context.getSystemService(ActivityManager::class.java)
        val power = context.getSystemService(PowerManager::class.java)
        val info = ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }
        val mem = MemorySnapshot.read()

        appendLine("HyPocket memory report")
        appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL}")
        appendLine("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
        appendLine("Power save mode: ${power.isPowerSaveMode}")
        appendLine("Low RAM device flag: ${am.isLowRamDevice}")
        appendLine("Memory class (heap limit per app): ${am.memoryClass} MB, large ${am.largeMemoryClass} MB")
        appendLine()
        appendLine("Total: ${mem.totalMb} MB")
        appendLine("Available: ${mem.availableMb} MB")
        appendLine("  free: ${mem.freeMb} MB")
        appendLine("  cached: ${mem.cachedMb} MB")
        appendLine("Swap (zram / RAM Plus): ${mem.swapTotalMb} MB, free ${mem.swapFreeMb} MB")
        appendLine("System says low memory: ${info.lowMemory} (threshold ${info.threshold / MB} MB)")
        appendLine()
        val own = Debug.MemoryInfo().also { Debug.getMemoryInfo(it) }
        appendLine("This app: ${own.totalPss / 1024} MB (PSS)")
        AppProcesses.list().forEach { appendLine("Process ${it.name} (${it.pid}): ${it.rssMb} MB") }
    }

    private const val MB = 1024L * 1024L
}
