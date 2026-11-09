package io.github.hypocketapp.core.auth

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import java.io.File

/**
 * Keeps the player's tokens in [file], encrypted with [cipher]. Tokens are
 * never written in plain text. If the file cannot be read back (the key was
 * removed, the file is damaged or from an older format), it is deleted and the
 * player simply signs in again.
 */
class EncryptedTokenStore(
    private val file: File,
    private val cipher: TokenCipher,
) : TokenStore {
    private val lock = Any()

    override fun load(): Tokens? = synchronized(lock) {
        if (!file.isFile) return null
        try {
            val bytes = file.readBytes()
            require(bytes.isNotEmpty() && bytes[0] == FORMAT_VERSION) { "unknown format" }
            val json = cipher.decrypt(bytes.copyOfRange(1, bytes.size)).decodeToString()
            Json.decodeFromString<StoredTokens>(json).toTokens()
        } catch (e: Exception) {
            // Never include the exception text: it could contain token data.
            deleteFile()
            null
        }
    }

    override fun save(tokens: Tokens): Unit = synchronized(lock) {
        val plain = Json.encodeToString(StoredTokens.from(tokens)).encodeToByteArray()
        val data = byteArrayOf(FORMAT_VERSION) + cipher.encrypt(plain)
        plain.fill(0)
        file.parentFile?.mkdirs()
        // Write to a temporary file first so a crash never leaves half a file.
        val temp = File(file.parentFile, "${file.name}.tmp")
        temp.writeBytes(data)
        if (!temp.renameTo(file)) {
            temp.delete()
            error("Could not save the sign-in")
        }
    }

    override fun clear(): Unit = synchronized(lock) {
        deleteFile()
        cipher.deleteKey()
    }

    private fun deleteFile() {
        file.delete()
        File(file.parentFile, "${file.name}.tmp").delete()
    }

    @Serializable
    private data class StoredTokens(
        val accessToken: String,
        val refreshToken: String? = null,
        val expiresAtMillis: Long,
    ) {
        fun toTokens() = Tokens(accessToken, refreshToken, expiresAtMillis)

        companion object {
            fun from(tokens: Tokens) = StoredTokens(tokens.accessToken, tokens.refreshToken, tokens.expiresAtMillis)
        }
    }

    private companion object {
        const val FORMAT_VERSION: Byte = 1
    }
}
