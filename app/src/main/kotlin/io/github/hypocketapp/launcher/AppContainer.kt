package io.github.hypocketapp.launcher

import android.content.Context
import io.github.hypocketapp.core.account.AccountSession
import io.github.hypocketapp.core.account.AccountVerifier
import io.github.hypocketapp.core.account.UnavailableLicenseChecker
import io.github.hypocketapp.core.auth.AuthConfig
import io.github.hypocketapp.core.auth.DeviceAuthClient
import io.github.hypocketapp.core.auth.InMemoryTokenStore
import io.github.hypocketapp.core.download.GameDownloader
import io.github.hypocketapp.core.download.UnavailableGameDownloader
import io.github.hypocketapp.core.launch.GameLauncher
import io.github.hypocketapp.core.settings.SettingsStore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob

/** Creates the app's long-lived objects. Kept by [HyPocketApplication]. */
class AppContainer(context: Context) {
    private val appScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    // No client id until Hypixel Studios registers one for HyPocket.
    private val authConfig = AuthConfig()

    val accountSession = AccountSession(
        isConfigured = authConfig.isConfigured,
        authClient = DeviceAuthClient(authConfig),
        verifier = AccountVerifier(UnavailableLicenseChecker()),
        tokenStore = InMemoryTokenStore(),
        scope = appScope,
    )

    val gameDownloader: GameDownloader = UnavailableGameDownloader()

    val gameLauncher = GameLauncher()

    val settingsStore = SettingsStore(context)
}
