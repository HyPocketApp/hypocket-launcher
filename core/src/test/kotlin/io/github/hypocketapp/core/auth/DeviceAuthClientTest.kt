package io.github.hypocketapp.core.auth

import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class DeviceAuthClientTest {
    private val config = AuthConfig(clientId = "test-client")

    /** Fake clock that only moves when the client sleeps. */
    private var now = 1_000_000L
    private val sleeps = mutableListOf<Long>()

    private fun client(transport: HttpTransport, config: AuthConfig = this.config) =
        DeviceAuthClient(
            config = config,
            transport = transport,
            clock = { now },
            sleep = { millis -> sleeps += millis; now += millis },
        )

    private fun authorization(expiresInSeconds: Long = 900) = DeviceAuthorization(
        deviceCode = "device-123",
        userCode = "ABCD-EFGH",
        verificationUri = "https://accounts.hytale.com/device",
        verificationUriComplete = null,
        expiresAtMillis = now + expiresInSeconds * 1000,
        pollIntervalSeconds = 5,
    )

    @Test
    fun `fails without a client id`() = runTest {
        val client = client({ _, _ -> error("must not call the network") }, AuthConfig())
        try {
            client.startAuthorization()
            fail("expected NotConfigured")
        } catch (e: AuthException.NotConfigured) {
            // expected
        }
    }

    @Test
    fun `parses the device authorization response`() = runTest {
        val sent = mutableMapOf<String, String>()
        val client = client({ url, form ->
            assertEquals(config.deviceAuthorizationUrl, url)
            sent += form
            HttpResponse(
                200,
                """{"device_code":"d","user_code":"U-1","verification_uri":"https://accounts.hytale.com/device","expires_in":900,"interval":5,"extra":1}""",
            )
        })

        val result = client.startAuthorization()

        assertEquals("test-client", sent["client_id"])
        assertEquals("openid offline", sent["scope"])
        assertEquals("U-1", result.userCode)
        assertEquals(now + 900_000, result.expiresAtMillis)
        assertEquals(5, result.pollIntervalSeconds)
    }

    @Test
    fun `polls until approved and slows down when asked`() = runTest {
        val answers = ArrayDeque(
            listOf(
                HttpResponse(400, """{"error":"authorization_pending"}"""),
                HttpResponse(400, """{"error":"slow_down"}"""),
                HttpResponse(200, """{"access_token":"a","refresh_token":"r","expires_in":3600}"""),
            ),
        )
        val client = client({ _, form ->
            assertEquals("urn:ietf:params:oauth:grant-type:device_code", form["grant_type"])
            answers.removeFirst()
        })

        val tokens = client.awaitTokens(authorization())

        assertEquals("a", tokens.accessToken)
        assertEquals("r", tokens.refreshToken)
        assertEquals(listOf(5_000L, 5_000L, 10_000L), sleeps)
    }

    @Test
    fun `reports when the player denies the sign-in`() = runTest {
        val client = client({ _, _ -> HttpResponse(400, """{"error":"access_denied"}""") })
        try {
            client.awaitTokens(authorization())
            fail("expected Denied")
        } catch (e: AuthException.Denied) {
            // expected
        }
    }

    @Test
    fun `stops polling when the code expires`() = runTest {
        val client = client({ _, _ -> HttpResponse(400, """{"error":"authorization_pending"}""") })
        try {
            client.awaitTokens(authorization(expiresInSeconds = 12))
            fail("expected Expired")
        } catch (e: AuthException.Expired) {
            assertTrue(sleeps.size <= 3)
        }
    }

    @Test
    fun `keeps the old refresh token when the server does not rotate it`() = runTest {
        val client = client({ _, form ->
            assertEquals("refresh_token", form["grant_type"])
            HttpResponse(200, """{"access_token":"new","expires_in":60}""")
        })

        val refreshed = client.refresh(Tokens("old", "keep-me", now))

        assertEquals("new", refreshed.accessToken)
        assertEquals("keep-me", refreshed.refreshToken)
    }

    @Test
    fun `turns a malformed response into a server error`() = runTest {
        val client = client({ _, _ -> HttpResponse(200, "not json") })
        try {
            client.startAuthorization()
            fail("expected Server")
        } catch (e: AuthException.Server) {
            assertEquals(200, e.status)
        }
    }
}
