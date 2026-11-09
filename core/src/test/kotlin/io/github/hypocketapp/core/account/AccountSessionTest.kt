package io.github.hypocketapp.core.account

import io.github.hypocketapp.core.auth.AuthConfig
import io.github.hypocketapp.core.auth.DeviceAuthClient
import io.github.hypocketapp.core.auth.HttpResponse
import io.github.hypocketapp.core.auth.HttpTransport
import io.github.hypocketapp.core.auth.InMemoryTokenStore
import io.github.hypocketapp.core.auth.Tokens
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class AccountSessionTest {
    private val now = 1_000_000L
    private val owned = LicenseChecker { LicenseResult.Owned("uuid-1", "Player") }

    private fun TestScope.session(
        store: InMemoryTokenStore,
        transport: HttpTransport = HttpTransport { _, _ -> error("must not call the network") },
        checker: LicenseChecker = owned,
    ): AccountSession {
        val dispatcher = UnconfinedTestDispatcher(testScheduler)
        return AccountSession(
            isConfigured = true,
            authClient = DeviceAuthClient(AuthConfig(clientId = "test-client"), transport, clock = { now }),
            verifier = AccountVerifier(checker, clock = { now }),
            tokenStore = store,
            scope = TestScope(dispatcher),
            storageDispatcher = dispatcher,
            clock = { now },
        )
    }

    @Test
    fun `saved sign-in is verified again on start`() = runTest {
        var checks = 0
        val store = InMemoryTokenStore().apply { save(Tokens("a", "r", now + 3_600_000)) }

        val session = session(store, checker = { checks++; LicenseResult.Owned("uuid-1", "Player") })

        assertEquals(1, checks)
        assertEquals("Player", session.verifiedAccount?.username)
    }

    @Test
    fun `saved sign-in for an account without the game is not verified`() = runTest {
        val store = InMemoryTokenStore().apply { save(Tokens("a", "r", now + 3_600_000)) }

        val session = session(store, checker = { LicenseResult.NotOwned })

        assertNull(session.verifiedAccount)
        assertEquals(SessionState.NotOwned, session.sessionState.value)
    }

    @Test
    fun `expired saved tokens are refreshed and saved`() = runTest {
        val store = InMemoryTokenStore().apply { save(Tokens("old", "refresh-1", now - 1)) }
        val transport = HttpTransport { _, form ->
            assertEquals("refresh_token", form["grant_type"])
            HttpResponse(200, """{"access_token":"new","refresh_token":"refresh-2","expires_in":3600}""")
        }

        val session = session(store, transport)

        assertTrue(session.sessionState.value is SessionState.Verified)
        assertEquals("new", store.load()?.accessToken)
        assertEquals("refresh-2", store.load()?.refreshToken)
    }

    @Test
    fun `rejected refresh clears the saved sign-in`() = runTest {
        val store = InMemoryTokenStore().apply { save(Tokens("old", "refresh-1", now - 1)) }
        val transport = HttpTransport { _, _ -> HttpResponse(400, """{"error":"invalid_grant"}""") }

        val session = session(store, transport)

        assertEquals(SessionState.SignedOut, session.sessionState.value)
        assertNull(store.load())
    }

    @Test
    fun `sign out forgets the saved sign-in`() = runTest {
        val store = InMemoryTokenStore().apply { save(Tokens("a", "r", now + 3_600_000)) }
        val session = session(store)

        session.signOut()

        assertNull(store.load())
        assertEquals(SessionState.SignedOut, session.sessionState.value)
    }
}
