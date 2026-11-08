package io.github.hypocketapp.core.auth

import java.io.File

/**
 * Checks on a real phone that [EncryptedTokenStore] with the Android Keystore
 * works: it saves made-up tokens under a separate key and file, reads them
 * back, checks they are not readable in the file, and deletes everything. It
 * never touches the player's real sign-in.
 *
 * Blocking: call it from a background thread.
 */
object SecureStorageCheck {
    enum class Result { PASSED, READ_BACK_FAILED, PLAIN_TEXT_FOUND, ERROR }

    fun run(dir: File): Result {
        val file = File(dir, "secure-storage-check.bin")
        val store = EncryptedTokenStore(file, KeystoreTokenCipher(alias = "hypocket-storage-check"))
        val sample = Tokens("check-access-0123456789", "check-refresh-0123456789", expiresAtMillis = 42L)
        return try {
            store.save(sample)
            val raw = file.readBytes().decodeToString()
            when {
                raw.contains("check-access") || raw.contains("check-refresh") -> Result.PLAIN_TEXT_FOUND
                store.load() != sample -> Result.READ_BACK_FAILED
                else -> Result.PASSED
            }
        } catch (e: Exception) {
            Result.ERROR
        } finally {
            runCatching { store.clear() }
        }
    }
}
