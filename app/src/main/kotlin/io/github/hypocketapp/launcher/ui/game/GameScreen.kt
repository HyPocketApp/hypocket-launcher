package io.github.hypocketapp.launcher.ui.game

import androidx.compose.material3.Button
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.hypocketapp.core.account.SessionState
import io.github.hypocketapp.core.download.DownloadState
import io.github.hypocketapp.core.launch.LaunchResult
import io.github.hypocketapp.launcher.AppContainer
import io.github.hypocketapp.launcher.R
import io.github.hypocketapp.launcher.ui.ScreenColumn
import kotlinx.coroutines.launch

@Composable
fun GameScreen(container: AppContainer, modifier: Modifier = Modifier) {
    val sessionState by container.accountSession.sessionState.collectAsStateWithLifecycle()
    val account = (sessionState as? SessionState.Verified)?.account
    val scope = rememberCoroutineScope()
    var download by remember { mutableStateOf<DownloadState?>(null) }
    var launch by remember { mutableStateOf<LaunchResult?>(null) }

    ScreenColumn(stringResource(R.string.game_title), modifier) {
        if (account == null) {
            Text(stringResource(R.string.game_sign_in_first))
            return@ScreenColumn
        }

        Button(onClick = {
            scope.launch {
                container.gameDownloader.download(account).collect { download = it }
            }
        }) {
            Text(stringResource(R.string.game_download))
        }

        when (val state = download) {
            is DownloadState.InProgress -> {
                val total = state.totalBytes
                if (total != null && total > 0) {
                    LinearProgressIndicator(progress = { state.downloadedBytes.toFloat() / total })
                } else {
                    LinearProgressIndicator()
                }
            }
            DownloadState.Completed -> Text(stringResource(R.string.game_download_completed))
            is DownloadState.Failed -> Text(stringResource(R.string.game_download_failed))
            is DownloadState.Unavailable -> Text(stringResource(R.string.game_download_unavailable))
            null -> Unit
        }

        Button(onClick = { launch = container.gameLauncher.launch(account) }) {
            Text(stringResource(R.string.game_play))
        }

        when (launch) {
            LaunchResult.Started, null -> Unit
            LaunchResult.UnsupportedDevice -> Text(stringResource(R.string.game_unsupported_device))
            LaunchResult.EngineNotReady -> Text(stringResource(R.string.game_engine_not_ready))
        }
    }
}
