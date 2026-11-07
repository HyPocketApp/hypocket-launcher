package io.github.hypocketapp.core.auth

/**
 * OAuth settings for signing in with the player's official account.
 *
 * [clientId] is intentionally empty. HyPocket must use a client registered
 * for this launcher by Hypixel Studios. Never put the official launcher's
 * client or the "hytale-server" client here: they were not issued to us.
 * While [clientId] is null, sign-in is unavailable and the game cannot start.
 */
data class AuthConfig(
    val clientId: String? = null,
    val scopes: List<String> = listOf("openid", "offline"),
    val deviceAuthorizationUrl: String = "https://oauth.accounts.hytale.com/oauth2/device/auth",
    val tokenUrl: String = "https://oauth.accounts.hytale.com/oauth2/token",
) {
    val isConfigured: Boolean get() = !clientId.isNullOrBlank()
}
