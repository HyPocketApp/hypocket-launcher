package io.github.hypocketapp.core.auth

/** Reasons the sign-in can fail. */
sealed class AuthException(message: String) : Exception(message) {
    /** No client registered for HyPocket yet. See [AuthConfig.clientId]. */
    class NotConfigured : AuthException("Sign-in is not configured")

    /** The player refused the sign-in in the browser. */
    class Denied : AuthException("Sign-in was denied")

    /** The code expired before the player approved it. */
    class Expired : AuthException("Sign-in code expired")

    /** Unexpected answer from the account service. */
    class Server(val status: Int, val error: String?) :
        AuthException("Account service error ($status${error?.let { ": $it" } ?: ""})")
}
