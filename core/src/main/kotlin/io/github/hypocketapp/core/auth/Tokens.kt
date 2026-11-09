package io.github.hypocketapp.core.auth

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

/** Tokens issued by the official account service. Never log these. */
data class Tokens(
    val accessToken: String,
    val refreshToken: String?,
    /** Epoch milliseconds after which [accessToken] must be refreshed. */
    val expiresAtMillis: Long,
) {
    fun isExpired(nowMillis: Long): Boolean = nowMillis >= expiresAtMillis

    override fun toString(): String = "Tokens(expiresAtMillis=$expiresAtMillis)"
}

/** What the player needs to approve the sign-in in their browser. */
data class DeviceAuthorization(
    val deviceCode: String,
    val userCode: String,
    val verificationUri: String,
    /** Link that already contains [userCode], when the server provides one. */
    val verificationUriComplete: String?,
    val expiresAtMillis: Long,
    val pollIntervalSeconds: Long,
)

@Serializable
internal data class DeviceAuthorizationResponse(
    @SerialName("device_code") val deviceCode: String,
    @SerialName("user_code") val userCode: String,
    @SerialName("verification_uri") val verificationUri: String,
    @SerialName("verification_uri_complete") val verificationUriComplete: String? = null,
    @SerialName("expires_in") val expiresIn: Long,
    @SerialName("interval") val interval: Long = 5,
)

@Serializable
internal data class TokenResponse(
    @SerialName("access_token") val accessToken: String,
    @SerialName("refresh_token") val refreshToken: String? = null,
    @SerialName("expires_in") val expiresIn: Long = 3600,
)

@Serializable
internal data class OAuthErrorResponse(
    @SerialName("error") val error: String,
    @SerialName("error_description") val description: String? = null,
)
