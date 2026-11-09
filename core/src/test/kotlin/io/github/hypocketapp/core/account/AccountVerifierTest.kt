package io.github.hypocketapp.core.account

import io.github.hypocketapp.core.auth.Tokens
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class AccountVerifierTest {
    private val now = 1_000L
    private val validTokens = Tokens("access", "refresh", expiresAtMillis = now + 60_000)

    @Test
    fun `default checker never verifies an account`() = runTest {
        val verifier = AccountVerifier(UnavailableLicenseChecker(), clock = { now })
        assertTrue(verifier.verify(validTokens) is VerificationResult.Unavailable)
    }

    @Test
    fun `account that does not own the game is rejected`() = runTest {
        val verifier = AccountVerifier({ LicenseResult.NotOwned }, clock = { now })
        assertEquals(VerificationResult.NotOwned, verifier.verify(validTokens))
    }

    @Test
    fun `expired session is rejected without asking the server`() = runTest {
        val verifier = AccountVerifier({ error("must not be called") }, clock = { now })
        val expired = validTokens.copy(expiresAtMillis = now)
        assertEquals(VerificationResult.SessionExpired, verifier.verify(expired))
    }

    @Test
    fun `owned game gives a verified account`() = runTest {
        val verifier = AccountVerifier({ LicenseResult.Owned("uuid-1", "Player") }, clock = { now })
        val result = verifier.verify(validTokens)
        assertTrue(result is VerificationResult.Verified)
        assertEquals("Player", (result as VerificationResult.Verified).account.username)
    }
}
