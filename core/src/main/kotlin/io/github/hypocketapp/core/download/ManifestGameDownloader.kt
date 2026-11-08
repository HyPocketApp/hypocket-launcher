package io.github.hypocketapp.core.download

import io.github.hypocketapp.core.account.VerifiedAccount
import io.github.hypocketapp.core.download.DownloadState.FailureReason
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.flowOn
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.security.MessageDigest

/**
 * Downloads the files of a [GameManifest] into [installDir]:
 *
 * - files already on the phone with the right size and hash are skipped;
 * - each file is downloaded to `<name>.part` and resumed from there after an
 *   interruption, then checked against its SHA-256 before it replaces the
 *   old file, so a broken download never ends up in the game folder;
 * - it checks free space first, and can wait for Wi-Fi.
 */
class ManifestGameDownloader(
    private val source: ManifestSource,
    private val fetcher: FileFetcher,
    private val installDir: File,
    private val freeBytes: () -> Long = { installDir.usableSpace },
    private val wifiOnly: () -> Boolean = { true },
    private val isOnWifi: () -> Boolean = { true },
) : GameDownloader {

    override fun download(account: VerifiedAccount): Flow<DownloadState> = flow {
        emit(DownloadState.Checking)
        if (wifiOnly() && !isOnWifi()) {
            emit(DownloadState.WaitingForWifi)
            return@flow
        }
        val manifest = try {
            source.fetch(account)
        } catch (e: ServerException) {
            emit(DownloadState.Failed(FailureReason.SERVER))
            return@flow
        } catch (e: IOException) {
            emit(DownloadState.Failed(FailureReason.NETWORK))
            return@flow
        }
        if (manifest == null) {
            emit(DownloadState.Unavailable("No official download source yet"))
            return@flow
        }

        val targets = try {
            manifest.files.map { it to safeTarget(it.path) }
        } catch (e: IllegalArgumentException) {
            emit(DownloadState.Failed(FailureReason.CORRUPTED))
            return@flow
        }
        val missing = targets.filterNot { (file, target) -> isValid(target, file) }
        val total = missing.sumOf { it.first.sizeBytes }
        val alreadyDownloaded = missing.sumOf { (_, target) -> partFile(target).takeIf { it.isFile }?.length() ?: 0L }
        val needed = total - alreadyDownloaded + SPACE_MARGIN_BYTES
        val free = freeBytes()
        if (needed > free) {
            emit(DownloadState.NotEnoughSpace(needed, free))
            return@flow
        }

        var done = 0L
        try {
            missing.forEachIndexed { index, (file, target) ->
                done = downloadOne(file, target, done, total, index, missing.size)
            }
        } catch (e: CancellationException) {
            throw e
        } catch (e: CorruptedException) {
            emit(DownloadState.Failed(FailureReason.CORRUPTED))
            return@flow
        } catch (e: ServerException) {
            emit(DownloadState.Failed(FailureReason.SERVER))
            return@flow
        } catch (e: StorageException) {
            emit(DownloadState.Failed(FailureReason.STORAGE))
            return@flow
        } catch (e: IOException) {
            emit(DownloadState.Failed(FailureReason.NETWORK))
            return@flow
        }
        emit(DownloadState.Completed)
    }.flowOn(Dispatchers.IO)

    /** Downloads one file and returns the bytes done so far across all files. */
    private suspend fun kotlinx.coroutines.flow.FlowCollector<DownloadState>.downloadOne(
        file: ManifestFile,
        target: File,
        doneBefore: Long,
        total: Long,
        index: Int,
        count: Int,
    ): Long {
        val part = partFile(target)
        part.parentFile?.mkdirs()
        var offset = part.takeIf { it.isFile }?.length() ?: 0L
        if (offset > file.sizeBytes) {
            part.delete()
            offset = 0
        }

        if (offset < file.sizeBytes) {
            val response = fetcher.open(file.url, offset)
            if (!response.resumed) offset = 0
            var written = offset
            var lastReport = 0L
            response.stream.use { input ->
                FileOutputStream(part, offset > 0).use { output ->
                    val buffer = ByteArray(BUFFER_BYTES)
                    while (true) {
                        currentCoroutineContext().ensureActive()
                        val read = input.read(buffer)
                        if (read < 0) break
                        written += read
                        if (written > file.sizeBytes) throw CorruptedException()
                        try {
                            output.write(buffer, 0, read)
                        } catch (e: IOException) {
                            throw StorageException(e)
                        }
                        if (written - lastReport >= REPORT_EVERY_BYTES) {
                            lastReport = written
                            emit(DownloadState.InProgress(doneBefore + written, total, index, count))
                        }
                    }
                }
            }
            if (written != file.sizeBytes) throw IOException("Download ended early")
        }

        emit(DownloadState.Verifying)
        if (sha256(part) != file.sha256.lowercase()) {
            part.delete()
            throw CorruptedException()
        }
        target.delete()
        if (!part.renameTo(target)) throw StorageException(IOException("rename failed"))
        val done = doneBefore + file.sizeBytes
        emit(DownloadState.InProgress(done, total, index + 1, count))
        return done
    }

    /** Resolves [path] inside [installDir], refusing anything that would leave it. */
    private fun safeTarget(path: String): File {
        require(path.isNotBlank() && !path.startsWith("/") && !path.contains('\\')) { "bad path" }
        require(path.split('/').none { it.isEmpty() || it == "." || it == ".." }) { "bad path" }
        val root = installDir.canonicalFile
        val target = File(root, path).canonicalFile
        require(target.path.startsWith(root.path + File.separator)) { "bad path" }
        return target
    }

    private fun isValid(target: File, file: ManifestFile): Boolean =
        target.isFile && target.length() == file.sizeBytes && sha256(target) == file.sha256.lowercase()

    private fun partFile(target: File) = File(target.parentFile, "${target.name}.part")

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(BUFFER_BYTES)
            while (true) {
                val read = input.read(buffer)
                if (read < 0) break
                digest.update(buffer, 0, read)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }

    private class CorruptedException : IOException("File does not match the expected hash or size")
    private class StorageException(cause: IOException) : IOException("Could not write the file", cause)

    private companion object {
        const val BUFFER_BYTES = 64 * 1024
        const val REPORT_EVERY_BYTES = 512L * 1024
        /** Keep some space free so the phone does not fill up completely. */
        const val SPACE_MARGIN_BYTES = 200L * 1024 * 1024
    }
}
