package io.github.hypocketapp.core.download

import io.github.hypocketapp.core.account.VerifiedAccount
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flowOf

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
    data class InProgress(val downloadedBytes: Long, val totalBytes: Long?) : DownloadState
    data object Completed : DownloadState
    data class Failed(val reason: String) : DownloadState
    data class Unavailable(val reason: String) : DownloadState
}

/** Used until there is a supported way to download the game client. */
class UnavailableGameDownloader : GameDownloader {
    override fun download(account: VerifiedAccount): Flow<DownloadState> =
        flowOf(DownloadState.Unavailable("Game download is not implemented yet"))
}
