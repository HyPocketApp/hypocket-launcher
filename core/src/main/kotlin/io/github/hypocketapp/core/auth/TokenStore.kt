package io.github.hypocketapp.core.auth

/** Where the player's tokens are kept between screens and app restarts. */
interface TokenStore {
    fun load(): Tokens?
    fun save(tokens: Tokens)
    fun clear()
}

/**
 * Keeps tokens in memory only, so the player signs in again after a restart.
 *
 * TODO: add a store encrypted with the Android Keystore. Tokens must never be
 *  saved in plain text.
 */
class InMemoryTokenStore : TokenStore {
    @Volatile
    private var tokens: Tokens? = null

    override fun load(): Tokens? = tokens

    override fun save(tokens: Tokens) {
        this.tokens = tokens
    }

    override fun clear() {
        tokens = null
    }
}
