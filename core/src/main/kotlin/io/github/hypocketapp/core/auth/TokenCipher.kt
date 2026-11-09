package io.github.hypocketapp.core.auth

import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/** Encrypts the saved tokens. */
interface TokenCipher {
    fun encrypt(plain: ByteArray): ByteArray
    fun decrypt(data: ByteArray): ByteArray

    /** Forgets the key, so data encrypted before can never be read again. */
    fun deleteKey()
}

/**
 * AES-256-GCM with a key that lives in the Android Keystore: the key never
 * leaves the secure hardware (or the system's keystore service) and cannot be
 * read by the app, so a copy of the token file is useless on its own.
 *
 * Output layout: 12-byte IV, then the ciphertext with its 16-byte tag.
 */
class KeystoreTokenCipher(private val alias: String = DEFAULT_ALIAS) : TokenCipher {

    override fun encrypt(plain: ByteArray): ByteArray {
        val cipher = Cipher.getInstance(TRANSFORMATION)
        cipher.init(Cipher.ENCRYPT_MODE, key())
        val iv = cipher.iv
        check(iv.size == IV_BYTES) { "unexpected IV size" }
        return iv + cipher.doFinal(plain)
    }

    override fun decrypt(data: ByteArray): ByteArray {
        require(data.size > IV_BYTES) { "too short" }
        val key = existingKey() ?: error("no key")
        val cipher = Cipher.getInstance(TRANSFORMATION)
        cipher.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(TAG_BITS, data, 0, IV_BYTES))
        return cipher.doFinal(data, IV_BYTES, data.size - IV_BYTES)
    }

    override fun deleteKey() {
        keyStore().takeIf { it.containsAlias(alias) }?.deleteEntry(alias)
    }

    private fun keyStore() = KeyStore.getInstance(ANDROID_KEYSTORE).apply { load(null) }

    private fun existingKey(): SecretKey? = keyStore().getKey(alias, null) as? SecretKey

    private fun key(): SecretKey = existingKey() ?: KeyGenerator
        .getInstance(KeyProperties.KEY_ALGORITHM_AES, ANDROID_KEYSTORE)
        .apply {
            init(
                KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setKeySize(256)
                    // The keystore picks a random IV for every encryption.
                    .setRandomizedEncryptionRequired(true)
                    .build(),
            )
        }
        .generateKey()

    private companion object {
        const val ANDROID_KEYSTORE = "AndroidKeyStore"
        const val DEFAULT_ALIAS = "hypocket-tokens"
        const val TRANSFORMATION = "AES/GCM/NoPadding"
        const val IV_BYTES = 12
        const val TAG_BITS = 128
    }
}
