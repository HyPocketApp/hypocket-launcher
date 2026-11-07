package io.github.hypocketapp.core.account

import io.github.hypocketapp.core.auth.Tokens

/** Turns signed-in tokens into a [VerifiedAccount], or explains why not. */
class AccountVerifier(
    private val licenseChecker: LicenseChecker,
    private val clock: () -> Long = System::currentTimeMillis,
) {
    suspend fun verify(tokens: Tokens): VerificationResult {
        if (tokens.isExpired(clock())) return VerificationResult.SessionExpired

        return when (val license = licenseChecker.check(tokens)) {
            is LicenseResult.Owned -> VerificationResult.Verified(
                VerifiedAccount(license.profileId, license.username, clock()),
            )
            LicenseResult.NotOwned -> VerificationResult.NotOwned
            is LicenseResult.Unavailable -> VerificationResult.Unavailable(license.reason)
        }
    }
}

sealed interface VerificationResult {
    data class Verified(val account: VerifiedAccount) : VerificationResult
    data object NotOwned : VerificationResult
    data object SessionExpired : VerificationResult
    data class Unavailable(val reason: String) : VerificationResult
}
