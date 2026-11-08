package io.github.hypocketapp.core.download

import io.github.hypocketapp.core.account.VerifiedAccount
import kotlinx.coroutines.flow.Flow

/**
 * Downloads the game files with the player's own account.
 *
 * HyPocket never ships or mirrors game files: they always come from the
 * official servers, for a [VerifiedAccount].
 */
interface GameDownloader {
    fun download(account: VerifiedAccount): Flow<DownloadState>
}

sealed interface DownloadState {
    /** Asking for the file list and checking files already on the phone. */
    data object Checking : DownloadState

    data class InProgress(
        val downloadedBytes: Long,
        val totalBytes: Long,
        val fileIndex: Int,
        val fileCount: Int,
    ) : DownloadState

    /** Checking the downloaded files against the expected hashes. */
    data object Verifying : DownloadState
    data object Completed : DownloadState

    /** The player chose to download on Wi-Fi only and the phone is on mobile data. */
    data object WaitingForWifi : DownloadState
    data class NotEnoughSpace(val neededBytes: Long, val freeBytes: Long) : DownloadState
    data class Failed(val reason: FailureReason) : DownloadState
    data class Unavailable(val reason: String) : DownloadState

    enum class FailureReason { NETWORK, CORRUPTED, SERVER, STORAGE }
}
