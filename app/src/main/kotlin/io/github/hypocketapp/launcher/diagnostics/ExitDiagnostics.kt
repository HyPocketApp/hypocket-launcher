package io.github.hypocketapp.launcher.diagnostics

import android.app.ActivityManager
import android.app.ApplicationExitInfo
import android.content.Context
import android.os.Build
import androidx.annotation.RequiresApi
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Finds out why the app's previous runs ended, so testers can report crashes
 * whose own report was lost: the system's exit records (native crash, killed
 * for memory, ...) and the stack trace of the last uncaught Kotlin/Java
 * exception. Nothing leaves the phone unless the tester copies it.
 */
object ExitDiagnostics {
    private const val CRASH_FILE = "last-crash.txt"

    /** Saves the stack trace of uncaught exceptions before the app dies. */
    fun install(context: Context) {
        val file = File(context.filesDir, CRASH_FILE)
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, error ->
            runCatching {
                file.writeText("${now()} thread ${thread.name}\n${error.stackTraceToString()}")
            }
            previous?.uncaughtException(thread, error)
        }
    }

    fun report(context: Context): String = buildString {
        appendLine("HyPocket last app exits")
        appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL}")
        appendLine("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")

        val am = context.getSystemService(ActivityManager::class.java)
        val memory = ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }
        appendLine("Memory now: ${memory.availMem / MB} MB free of ${memory.totalMem / MB} MB")

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            appendExits(context, am)
        } else {
            appendLine("Exit records need Android 11 or newer.")
        }

        val crash = File(context.filesDir, CRASH_FILE)
        if (crash.isFile) {
            appendLine()
            appendLine("--- last uncaught exception ---")
            append(crash.readText().take(MAX_TRACE))
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun StringBuilder.appendExits(context: Context, am: ActivityManager) {
        val exits = am.getHistoricalProcessExitReasons(context.packageName, 0, 5)
        if (exits.isEmpty()) appendLine("No exits recorded yet.")
        exits.forEach { exit ->
            appendLine()
            appendLine("${format(exit.timestamp)}: ${reasonName(exit.reason)}")
            exit.description?.let { appendLine("  description: $it") }
            if (exit.reason == ApplicationExitInfo.REASON_SIGNALED ||
                exit.reason == ApplicationExitInfo.REASON_CRASH_NATIVE
            ) {
                appendLine("  signal: ${exit.status}")
            } else {
                appendLine("  status: ${exit.status}")
            }
            appendLine("  memory at exit: pss ${exit.pss / 1024} MB, rss ${exit.rss / 1024} MB")
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun reasonName(reason: Int): String = when (reason) {
        ApplicationExitInfo.REASON_EXIT_SELF -> "closed itself"
        ApplicationExitInfo.REASON_SIGNALED -> "killed by a signal"
        ApplicationExitInfo.REASON_LOW_MEMORY -> "killed by the system for low memory"
        ApplicationExitInfo.REASON_CRASH -> "crash (Kotlin/Java exception)"
        ApplicationExitInfo.REASON_CRASH_NATIVE -> "crash (native code)"
        ApplicationExitInfo.REASON_ANR -> "not responding (ANR)"
        ApplicationExitInfo.REASON_INITIALIZATION_FAILURE -> "failed to start"
        ApplicationExitInfo.REASON_PERMISSION_CHANGE -> "permission changed"
        ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE -> "killed for excessive resource use"
        ApplicationExitInfo.REASON_USER_REQUESTED -> "stopped by the user"
        ApplicationExitInfo.REASON_USER_STOPPED -> "user stopped"
        ApplicationExitInfo.REASON_DEPENDENCY_DIED -> "a dependency died"
        ApplicationExitInfo.REASON_OTHER -> "other (system)"
        else -> "unknown ($reason)"
    }

    private fun now() = format(System.currentTimeMillis())

    private fun format(millis: Long) =
        SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.ROOT).format(Date(millis))

    private const val MB = 1024L * 1024L
    private const val MAX_TRACE = 6000
}
