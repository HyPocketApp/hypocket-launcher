package io.github.hypocketapp.core.account

/**
 * Proof that the player signed in and owns the game.
 *
 * The constructor is internal: only [AccountVerifier] can create one, so code
 * outside the core module cannot fake a verified account. Downloading and
 * launching the game require an instance of this class.
 */
class VerifiedAccount internal constructor(
    val profileId: String,
    val username: String,
    val verifiedAtMillis: Long,
) {
    override fun toString(): String = "VerifiedAccount(username=$username)"
}
