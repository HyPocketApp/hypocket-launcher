package io.github.hypocketapp.core.auth

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class EncryptedTokenStoreTest {
    @get:Rule
    val folder = TemporaryFolder()

    /** Reversible stand-in for the Keystore cipher, which only exists on Android. */
    private class FakeCipher : TokenCipher {
        var key: Byte? = 0x5A
        override fun encrypt(plain: ByteArray) = plain.map { (it.toInt() xor key!!.toInt()).toByte() }.toByteArray()
        override fun decrypt(data: ByteArray): ByteArray {
            val k = key ?: error("no key")
            return data.map { (it.toInt() xor k.toInt()).toByte() }.toByteArray()
        }
        override fun deleteKey() {
            key = null
        }
    }

    private val tokens = Tokens("secret-access", "secret-refresh", expiresAtMillis = 123_456L)

    @Test
    fun `saved tokens come back after a restart`() {
        val file = folder.root.resolve("sign-in.bin")
        val cipher = FakeCipher()
        EncryptedTokenStore(file, cipher).save(tokens)

        assertEquals(tokens, EncryptedTokenStore(file, cipher).load())
    }

    @Test
    fun `tokens are never written in plain text`() {
        val file = folder.root.resolve("sign-in.bin")
        EncryptedTokenStore(file, FakeCipher()).save(tokens)

        val raw = file.readBytes().decodeToString()
        assertFalse(raw.contains("secret-access"))
        assertFalse(raw.contains("secret-refresh"))
    }

    @Test
    fun `unreadable file is deleted and gives no tokens`() {
        val file = folder.root.resolve("sign-in.bin")
        val cipher = FakeCipher()
        EncryptedTokenStore(file, cipher).save(tokens)
        file.writeBytes(byteArrayOf(1, 2, 3))

        assertNull(EncryptedTokenStore(file, cipher).load())
        assertFalse(file.exists())
    }

    @Test
    fun `clear removes the file and the key`() {
        val file = folder.root.resolve("sign-in.bin")
        val cipher = FakeCipher()
        val store = EncryptedTokenStore(file, cipher)
        store.save(tokens)

        store.clear()

        assertFalse(file.exists())
        assertNull(cipher.key)
        assertNull(store.load())
    }

    @Test
    fun `missing key gives no tokens`() {
        val file = folder.root.resolve("sign-in.bin")
        val cipher = FakeCipher()
        EncryptedTokenStore(file, cipher).save(tokens)
        cipher.key = null

        assertNull(EncryptedTokenStore(file, cipher).load())
        assertTrue(!file.exists())
    }
}
