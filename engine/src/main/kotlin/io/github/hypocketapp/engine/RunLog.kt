package io.github.hypocketapp.engine

import android.content.ClipData
import android.content.Context
import android.content.Intent
import androidx.core.content.FileProvider
import java.io.File
import java.io.Writer
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * A report written to the phone while a run goes on, so a crash, a closed app
 * or a killed process does not lose it. Each line is flushed as it comes.
 *
 * [finish] replaces the live lines with the run's final report. A log that
 * never got there keeps its header and every line up to where it stopped,
 * under [INTERRUPTED_MARKER]. Not thread-confined: lines come from the game's
 * and the server's output threads.
 */
class RunLog internal constructor(val file: File, header: String) : AutoCloseable {
    private var writer: Writer? = file.bufferedWriter()
    private var bytes = 0L

    init {
        line(INTERRUPTED_MARKER)
        header.trimEnd().lines().forEach(::line)
    }

    /** Appends one line, unless the log is full or finished. */
    @Synchronized
    fun line(text: String) {
        val out = writer ?: return
        if (bytes > MAX_BYTES) return
        runCatching {
            out.write(text)
            out.write("\n")
            out.flush()
        }
        bytes += text.length + 1
        if (bytes > MAX_BYTES) runCatching { out.write("[log full, later lines dropped]\n"); out.flush() }
    }

    @Synchronized
    fun section(title: String) = line("--- $title ---")

    /** Replaces the live lines with [report], the run's complete report. */
    @Synchronized
    fun finish(report: String) {
        close()
        val tmp = File(file.parentFile, file.name + ".tmp")
        runCatching {
            tmp.writeText(report)
            if (!tmp.renameTo(file)) tmp.delete()
        }
    }

    @Synchronized
    override fun close() {
        runCatching { writer?.close() }
        writer = null
    }

    companion object {
        /** First line of a log whose run did not reach its final report. */
        const val INTERRUPTED_MARKER = "[This run did not finish. Below is everything it wrote until it stopped.]"
        private const val MAX_BYTES = 2L * 1024 * 1024
    }
}

/**
 * The reports kept on the phone: in the app's private storage, newest first,
 * the oldest deleted beyond [MAX_LOGS].
 */
class RunLogs(context: Context) {
    private val appContext = context.applicationContext
    val dir: File = File(appContext.filesDir, "reports")

    class Entry(val file: File) {
        /** What ran: "blocks", "benchmark" and so on. */
        val kind: String get() = file.nameWithoutExtension.drop(STAMP_LENGTH + 1).substringBefore('_')

        /** When the run started, from the file's name. */
        val startedAt: Long
            get() = runCatching { stampFormat().parse(file.name.take(STAMP_LENGTH))?.time }.getOrNull() ?: file.lastModified()
        val sizeBytes: Long get() = file.length()

        /** False while it runs, or if the run was cut off before its final report. */
        val finished: Boolean
            get() = runCatching { file.bufferedReader().use { it.readLine() } != RunLog.INTERRUPTED_MARKER }.getOrDefault(true)

        fun read(): String = runCatching { file.readText() }.getOrDefault("")
    }

    /** Starts a log for a run of [kind], beginning with [header]. */
    fun start(kind: String, header: String): RunLog {
        dir.mkdirs()
        prune(MAX_LOGS - 1)
        val safeKind = kind.lowercase(Locale.ROOT).replace(Regex("[^a-z0-9]+"), "-").trim('-').ifEmpty { "run" }
        val base = "${stampFormat().format(Date())}-$safeKind"
        var file = File(dir, "$base.txt")
        var n = 2
        while (file.exists()) file = File(dir, "${base}_${n++}.txt")
        return RunLog(file, header)
    }

    fun list(): List<Entry> =
        dir.listFiles { f -> f.isFile && f.name.endsWith(".txt") }.orEmpty()
            .sortedByDescending { it.name }
            .map(::Entry)

    /**
     * An intent that sends [entry] as a .txt file, whole, for
     * Intent.createChooser. Pasted text is cut by the share sheet and the
     * clipboard on large reports; a file is not, and whoever gets it can
     * search it instead of reading it all.
     */
    fun shareIntent(entry: Entry): Intent {
        val uri = FileProvider.getUriForFile(appContext, "${appContext.packageName}$AUTHORITY_SUFFIX", entry.file)
        return Intent(Intent.ACTION_SEND)
            .setType("text/plain")
            .putExtra(Intent.EXTRA_STREAM, uri)
            .putExtra(Intent.EXTRA_SUBJECT, entry.file.name)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            // The chooser passes the read permission on only through clip data.
            .also { it.clipData = ClipData.newRawUri(entry.file.name, uri) }
    }

    fun deleteAll() {
        dir.listFiles().orEmpty().forEach { it.delete() }
    }

    private fun prune(keep: Int) {
        list().drop(keep.coerceAtLeast(0)).forEach { it.file.delete() }
    }

    private companion object {
        const val MAX_LOGS = 30
        const val STAMP_LENGTH = 15

        /** After the package name; must match the provider in the engine's manifest. */
        const val AUTHORITY_SUFFIX = ".hypocket.reports"

        // yyyyMMdd-HHmmss: sorts by name in time order.
        fun stampFormat() = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT)
    }
}
