package io.github.hypocketapp.core.account

import io.github.hypocketapp.core.auth.AuthException
import io.github.hypocketapp.core.auth.DeviceAuthClient
import io.github.hypocketapp.core.auth.DeviceAuthorization
import io.github.hypocketapp.core.auth.TokenStore
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.io.IOException

/** Runs the whole sign-in: device code, approval, then the license check. */
class AccountSession(
    private val isConfigured: Boolean,
    private val authClient: DeviceAuthClient,
    private val verifier: AccountVerifier,
    private val tokenStore: TokenStore,
    private val scope: CoroutineScope,
) {
    private val state = MutableStateFlow(initialState())
    val sessionState: StateFlow<SessionState> = state.asStateFlow()

    private var job: Job? = null

    /** The verified account, or null if the player may not play right now. */
    val verifiedAccount: VerifiedAccount?
        get() = (state.value as? SessionState.Verified)?.account

    fun signIn() {
        if (!isConfigured) {
            state.value = SessionState.NotConfigured
            return
        }
        job?.cancel()
        job = scope.launch {
            try {
                val authorization = authClient.startAuthorization()
                state.value = SessionState.WaitingForApproval(authorization)
                val tokens = authClient.awaitTokens(authorization)
                tokenStore.save(tokens)
                state.value = SessionState.Verifying
                state.value = verifier.verify(tokens).toSessionState()
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
        tokenStore.clear()
        state.value = initialState()
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
