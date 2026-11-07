package io.github.hypocketapp.launcher.ui.account

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import androidx.core.net.toUri
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.hypocketapp.core.account.AccountSession
import io.github.hypocketapp.core.account.FailureReason
import io.github.hypocketapp.core.account.SessionState
import io.github.hypocketapp.launcher.R
import io.github.hypocketapp.launcher.ui.ScreenColumn

@Composable
fun AccountScreen(session: AccountSession, modifier: Modifier = Modifier) {
    val state by session.sessionState.collectAsStateWithLifecycle()
    val context = LocalContext.current

    ScreenColumn(stringResource(R.string.account_title), modifier) {
        Text(stringResource(R.string.account_requirement))

        when (val current = state) {
            SessionState.NotConfigured -> Text(stringResource(R.string.account_not_configured))

            SessionState.SignedOut -> Button(onClick = session::signIn) {
                Text(stringResource(R.string.account_sign_in))
            }

            is SessionState.WaitingForApproval -> {
                val authorization = current.authorization
                Text(stringResource(R.string.account_enter_code))
                Text(
                    authorization.userCode,
                    style = MaterialTheme.typography.displaySmall,
                    fontFamily = FontFamily.Monospace,
                )
                Text(authorization.verificationUri)
                Button(onClick = {
                    openInBrowser(context, authorization.verificationUriComplete ?: authorization.verificationUri)
                }) {
                    Text(stringResource(R.string.account_open_sign_in_page))
                }
                OutlinedButton(onClick = session::cancel) {
                    Text(stringResource(R.string.action_cancel))
                }
            }

            SessionState.Verifying -> {
                CircularProgressIndicator()
                Text(stringResource(R.string.account_verifying))
            }

            is SessionState.Verified -> {
                Text(stringResource(R.string.account_signed_in_as, current.account.username))
                OutlinedButton(onClick = session::signOut) {
                    Text(stringResource(R.string.account_sign_out))
                }
            }

            SessionState.NotOwned -> RetryMessage(R.string.account_not_owned, session)

            SessionState.LicenseCheckUnavailable ->
                RetryMessage(R.string.account_license_check_unavailable, session)

            is SessionState.Failed -> RetryMessage(current.reason.message(), session)
        }
    }
}

@Composable
private fun RetryMessage(message: Int, session: AccountSession) {
    Text(stringResource(message), color = MaterialTheme.colorScheme.error)
    Button(onClick = session::signIn) {
        Text(stringResource(R.string.action_try_again))
    }
}

private fun FailureReason.message(): Int = when (this) {
    FailureReason.DENIED -> R.string.account_error_denied
    FailureReason.EXPIRED -> R.string.account_error_expired
    FailureReason.SERVER -> R.string.account_error_server
    FailureReason.NETWORK -> R.string.account_error_network
}

/** Opens the official sign-in page in the player's own browser. */
private fun openInBrowser(context: Context, url: String) {
    try {
        context.startActivity(Intent(Intent.ACTION_VIEW, url.toUri()))
    } catch (e: ActivityNotFoundException) {
        // No browser installed: the link is shown on screen so the player can
        // open it on another device.
    }
}
