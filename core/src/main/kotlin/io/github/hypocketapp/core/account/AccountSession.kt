package io.github.hypocketapp.core.account

import io.github.hypocketapp.core.auth.AuthException
import io.github.hypocketapp.core.auth.DeviceAuthClient
import io.github.hypocketapp.core.auth.DeviceAuthorization
import io.github.hypocketapp.core.auth.TokenStore
import io.github.hypocketapp.core.auth.Tokens
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.IOException

/**
 * Runs the whole sign-in: device code, approval, then the license check.
 *
 * A saved sign-in is resumed when the app starts, but it always goes through
 * [AccountVerifier] again: saved tokens alone never make an account verified.
 */
class AccountSession(
    private val isConfigured: Boolean,
    private val authClient: DeviceAuthClient,
    private val verifier: AccountVerifier,
    private val tokenStore: TokenStore,
    private val scope: CoroutineScope,
    private val storageDispatcher: CoroutineDispatcher = Dispatchers.IO,
    private val clock: () -> Long = System::currentTimeMillis,
) {
    private val state = MutableStateFlow(initialState())
    val sessionState: StateFlow<SessionState> = state.asStateFlow()

    private var job: Job? = null

    init {
        if (isConfigured) launchSignIn { loadSaved()?.let { resume(it) } }
    }

    /** The verified account, or null if the player may not play right now. */
    val verifiedAccount: VerifiedAccount?
        get() = (state.value as? SessionState.Verified)?.account

    /** Signs in, or retries a saved sign-in that failed for a temporary reason. */
    fun signIn() {
        if (!isConfigured) {
            state.value = SessionState.NotConfigured
            return
        }
        launchSignIn {
            val saved = loadSaved()
            if (saved != null) {
                resume(saved)
            } else {
                val authorization = authClient.startAuthorization()
                state.value = SessionState.WaitingForApproval(authorization)
                val tokens = authClient.awaitTokens(authorization)
                withContext(storageDispatcher) { tokenStore.save(tokens) }
                state.value = SessionState.Verifying
                state.value = verifier.verify(tokens).toSessionState()
            }
        }
    }

    private suspend fun loadSaved(): Tokens? = withContext(storageDispatcher) { tokenStore.load() }

    /** Refreshes saved tokens when needed, then verifies the account again. */
    private suspend fun resume(saved: Tokens) {
        state.value = SessionState.Verifying
        val tokens = if (saved.isExpired(clock() + REFRESH_MARGIN_MILLIS)) {
            try {
                authClient.refresh(saved).also { withContext(storageDispatcher) { tokenStore.save(it) } }
            } catch (e: AuthException.Expired) {
                // The saved sign-in can no longer be used: start over.
                withContext(storageDispatcher) { tokenStore.clear() }
                state.value = SessionState.SignedOut
                return
            }
        } else {
            saved
        }
        state.value = verifier.verify(tokens).toSessionState()
    }

    private fun launchSignIn(block: suspend () -> Unit) {
        job?.cancel()
        job = scope.launch {
            try {
                block()
            } catch (e: CancellationException) {
                throw e
            } catch (e: AuthException.NotConfigured) {
                state.value = SessionState.NotConfigured
            } catch (e: AuthException.Denied) {
                state.value = SessionState.Failed(FailureReason.DENIED)
            } catch (e: AuthException.Expired) {
                state.value = SessionState.Failed(FailureReason.EXPIRED)
            } catch (e: AuthException.Server) {
                state.value = SessionState.Failed(FailureReason.SERVER)
            } catch (e: IOException) {
                state.value = SessionState.Failed(FailureReason.NETWORK)
            }
        }
    }

    fun cancel() {
        job?.cancel()
        job = null
        state.value = initialState()
    }

    fun signOut() {
        cancel()
        scope.launch { withContext(storageDispatcher) { tokenStore.clear() } }
    }

    private companion object {
        /** Refresh a little before the access token expires. */
        const val REFRESH_MARGIN_MILLIS = 60_000L
    }

    private fun initialState() =
        if (isConfigured) SessionState.SignedOut else SessionState.NotConfigured

    private fun VerificationResult.toSessionState(): SessionState = when (this) {
        is VerificationResult.Verified -> SessionState.Verified(account)
        VerificationResult.NotOwned -> SessionState.NotOwned
        VerificationResult.SessionExpired -> SessionState.Failed(FailureReason.EXPIRED)
        is VerificationResult.Unavailable -> SessionState.LicenseCheckUnavailable
    }
}

sealed interface SessionState {
    /** No OAuth client registered for HyPocket yet. */
    data object NotConfigured : SessionState
    data object SignedOut : SessionState
    data class WaitingForApproval(val authorization: DeviceAuthorization) : SessionState
    data object Verifying : SessionState
    data class Verified(val account: VerifiedAccount) : SessionState
    data object NotOwned : SessionState
    data object LicenseCheckUnavailable : SessionState
    data class Failed(val reason: FailureReason) : SessionState
}

enum class FailureReason { DENIED, EXPIRED, SERVER, NETWORK }
