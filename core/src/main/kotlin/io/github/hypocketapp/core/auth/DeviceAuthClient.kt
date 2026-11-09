package io.github.hypocketapp.core.auth

import kotlinx.coroutines.delay
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.Json

/**
 * Signs the player in with the OAuth 2.0 Device Authorization Grant
 * (RFC 8628). The player approves the sign-in on the official website in
 * their own browser, so HyPocket never sees their email or password.
 */
class DeviceAuthClient(
    private val config: AuthConfig,
    private val transport: HttpTransport = UrlConnectionTransport(),
    private val clock: () -> Long = System::currentTimeMillis,
    private val sleep: suspend (Long) -> Unit = { delay(it) },
) {
    private val json = Json { ignoreUnknownKeys = true }

    /** Step 1: asks the account service for a code to show to the player. */
    suspend fun startAuthorization(): DeviceAuthorization {
        val clientId = requireClientId()
        val response = transport.postForm(
            config.deviceAuthorizationUrl,
            mapOf("client_id" to clientId, "scope" to config.scopes.joinToString(" ")),
        )
        if (response.status !in 200..299) throw serverError(response)

        val body = decode<DeviceAuthorizationResponse>(response)
        return DeviceAuthorization(
            deviceCode = body.deviceCode,
            userCode = body.userCode,
            verificationUri = body.verificationUri,
            verificationUriComplete = body.verificationUriComplete,
            expiresAtMillis = clock() + body.expiresIn * 1000,
            pollIntervalSeconds = body.interval.coerceAtLeast(1),
        )
    }

    /** Step 2: waits until the player approves, refuses or the code expires. */
    suspend fun awaitTokens(authorization: DeviceAuthorization): Tokens {
        val clientId = requireClientId()
        var intervalSeconds = authorization.pollIntervalSeconds

        while (clock() < authorization.expiresAtMillis) {
            sleep(intervalSeconds * 1000)
            val response = transport.postForm(
                config.tokenUrl,
                mapOf(
                    "grant_type" to DEVICE_CODE_GRANT,
                    "device_code" to authorization.deviceCode,
                    "client_id" to clientId,
                ),
            )
            if (response.status in 200..299) return toTokens(decode(response))

            when (errorCode(response)) {
                "authorization_pending" -> Unit
                "slow_down" -> intervalSeconds += SLOW_DOWN_STEP_SECONDS
                "access_denied" -> throw AuthException.Denied()
                "expired_token" -> throw AuthException.Expired()
                else -> throw serverError(response)
            }
        }
        throw AuthException.Expired()
    }

    /** Gets a new access token without asking the player to sign in again. */
    suspend fun refresh(tokens: Tokens): Tokens {
        val clientId = requireClientId()
        val refreshToken = tokens.refreshToken ?: throw AuthException.Expired()
        val response = transport.postForm(
            config.tokenUrl,
            mapOf(
                "grant_type" to "refresh_token",
                "refresh_token" to refreshToken,
                "client_id" to clientId,
            ),
        )
        if (response.status !in 200..299) {
            if (errorCode(response) == "invalid_grant") throw AuthException.Expired()
            throw serverError(response)
        }
        val refreshed = toTokens(decode(response))
        // Some servers do not rotate refresh tokens; keep the old one then.
        return refreshed.copy(refreshToken = refreshed.refreshToken ?: refreshToken)
    }

    private fun requireClientId(): String =
        config.clientId?.takeIf { it.isNotBlank() } ?: throw AuthException.NotConfigured()

    private fun toTokens(body: TokenResponse) = Tokens(
        accessToken = body.accessToken,
        refreshToken = body.refreshToken,
        expiresAtMillis = clock() + body.expiresIn * 1000,
    )

    private inline fun <reified T> decode(response: HttpResponse): T = try {
        json.decodeFromString<T>(response.body)
    } catch (e: SerializationException) {
        throw AuthException.Server(response.status, "invalid response")
    } catch (e: IllegalArgumentException) {
        throw AuthException.Server(response.status, "invalid response")
    }

    private fun errorCode(response: HttpResponse): String? = try {
        json.decodeFromString<OAuthErrorResponse>(response.body).error
    } catch (e: SerializationException) {
        null
    } catch (e: IllegalArgumentException) {
        null
    }

    private fun serverError(response: HttpResponse) =
        AuthException.Server(response.status, errorCode(response))

    private companion object {
        const val DEVICE_CODE_GRANT = "urn:ietf:params:oauth:grant-type:device_code"
        const val SLOW_DOWN_STEP_SECONDS = 5L
    }
}
