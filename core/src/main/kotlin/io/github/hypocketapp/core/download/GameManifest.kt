package io.github.hypocketapp.core.download

import io.github.hypocketapp.core.account.VerifiedAccount

/** The game files of one version, as the official servers list them. */
data class GameManifest(
    val version: String,
    val files: List<ManifestFile>,
)

data class ManifestFile(
    /** Path inside the game folder, with forward slashes. */
    val path: String,
    val url: String,
    val sizeBytes: Long,
    /** Lowercase hex SHA-256 of the whole file. */
    val sha256: String,
)

/** Gets the list of game files for a verified account from the official servers. */
fun interface ManifestSource {
    /** Returns null when downloading is not possible (see [UnavailableManifestSource]). */
    suspend fun fetch(account: VerifiedAccount): GameManifest?
}

/**
 * Used until there is a supported, official way to get the game files for a
 * player's account. HyPocket never uses unofficial mirrors.
 */
class UnavailableManifestSource : ManifestSource {
    override suspend fun fetch(account: VerifiedAccount): GameManifest? = null
}
