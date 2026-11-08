package io.github.hypocketapp.launcher

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import io.github.hypocketapp.core.account.AccountSession
import io.github.hypocketapp.core.account.AccountVerifier
import io.github.hypocketapp.core.account.UnavailableLicenseChecker
import io.github.hypocketapp.core.auth.AuthConfig
import io.github.hypocketapp.core.auth.DeviceAuthClient
import io.github.hypocketapp.core.auth.EncryptedTokenStore
import io.github.hypocketapp.core.auth.KeystoreTokenCipher
import io.github.hypocketapp.core.download.GameDownloader
import io.github.hypocketapp.core.download.ManifestGameDownloader
import io.github.hypocketapp.core.download.UnavailableManifestSource
import io.github.hypocketapp.core.download.UrlConnectionFetcher
import io.github.hypocketapp.core.launch.GameLauncher
import io.github.hypocketapp.core.settings.SettingsStore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import java.io.File

/** Creates the app's long-lived objects. Kept by [HyPocketApplication]. */
class AppContainer(context: Context) {
    private val appScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    // No client id until Hypixel Studios registers one for HyPocket.
    private val authConfig = AuthConfig()

    val accountSession = AccountSession(
        isConfigured = authConfig.isConfigured,
        authClient = DeviceAuthClient(authConfig),
        verifier = AccountVerifier(UnavailableLicenseChecker()),
        // noBackupFilesDir: the encrypted sign-in is never copied off the phone.
        tokenStore = EncryptedTokenStore(File(context.noBackupFilesDir, "sign-in.bin"), KeystoreTokenCipher()),
        scope = appScope,
    )

    val settingsStore = SettingsStore(context)

    // No official download source yet: UnavailableManifestSource reports that.
    val gameDownloader: GameDownloader = ManifestGameDownloader(
        source = UnavailableManifestSource(),
        fetcher = UrlConnectionFetcher(),
        installDir = File(context.filesDir, "game"),
        wifiOnly = { settingsStore.settings.value.downloadOnWifiOnly },
        isOnWifi = { isOnUnmeteredNetwork(context) },
    )

    val gameLauncher = GameLauncher()

    private fun isOnUnmeteredNetwork(context: Context): Boolean {
        val connectivity = context.getSystemService(ConnectivityManager::class.java)
        val capabilities = connectivity.getNetworkCapabilities(connectivity.activeNetwork) ?: return false
        return capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED)
    }
}
