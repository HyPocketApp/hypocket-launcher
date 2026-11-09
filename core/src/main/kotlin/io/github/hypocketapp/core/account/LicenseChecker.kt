package io.github.hypocketapp.core.account

import io.github.hypocketapp.core.auth.Tokens

/** Asks the official services whether a signed-in account owns the game. */
fun interface LicenseChecker {
    suspend fun check(tokens: Tokens): LicenseResult
}

sealed interface LicenseResult {
    /** The account owns the game and has this game profile. */
    data class Owned(val profileId: String, val username: String) : LicenseResult

    /** The account is valid but does not own the game. */
    data object NotOwned : LicenseResult

    /** The check could not be done (no API, network error, ...). */
    data class Unavailable(val reason: String) : LicenseResult
}

/**
 * Used until there is a supported way to read the player's game profile.
 * It always reports [LicenseResult.Unavailable], so no account is ever
 * treated as verified by default.
 */
class UnavailableLicenseChecker : LicenseChecker {
    override suspend fun check(tokens: Tokens): LicenseResult =
        LicenseResult.Unavailable("License check is not implemented yet")
}
