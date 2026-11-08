package io.github.hypocketapp.launcher.ui.game

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.OutlinedButton
import androidx.compose.ui.unit.dp
import androidx.compose.material3.Button
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.hypocketapp.core.account.SessionState
import io.github.hypocketapp.core.download.DownloadState
import io.github.hypocketapp.core.launch.LaunchResult
import io.github.hypocketapp.launcher.AppContainer
import io.github.hypocketapp.launcher.R
import io.github.hypocketapp.launcher.ui.ScreenColumn
import io.github.hypocketapp.engine.DeviceCheck
import io.github.hypocketapp.engine.DeviceProblem
import io.github.hypocketapp.engine.DeviceReport
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun GameScreen(container: AppContainer, modifier: Modifier = Modifier) {
    val sessionState by container.accountSession.sessionState.collectAsStateWithLifecycle()
    val account = (sessionState as? SessionState.Verified)?.account
    val scope = rememberCoroutineScope()
    var download by remember { mutableStateOf<DownloadState?>(null) }
    var downloadJob by remember { mutableStateOf<Job?>(null) }
    var launch by remember { mutableStateOf<LaunchResult?>(null) }
    val context = LocalContext.current
    val device by produceState<DeviceReport?>(null) {
        value = withContext(Dispatchers.IO) { DeviceCheck.run(context) }
    }

    ScreenColumn(stringResource(R.string.game_title), modifier) {
        DeviceCheckCard(device)

        if (account == null) {
            Text(stringResource(R.string.game_sign_in_first))
            return@ScreenColumn
        }

        DownloadSection(
            state = download,
            running = downloadJob?.isActive == true,
            onStart = {
                downloadJob?.cancel()
                downloadJob = scope.launch {
                    container.gameDownloader.download(account).collect { download = it }
                }
            },
            onCancel = {
                downloadJob?.cancel()
                downloadJob = null
                download = null
            },
        )

        val report = device
        Button(enabled = report != null, onClick = {
            if (report != null) launch = container.gameLauncher.launch(account, report)
        }) {
            Text(stringResource(R.string.game_play))
        }

        when (launch) {
            LaunchResult.Started, null -> Unit
            LaunchResult.UnsupportedDevice -> Text(stringResource(R.string.game_unsupported_device))
            LaunchResult.EngineNotReady -> Text(stringResource(R.string.game_engine_not_ready))
        }
    }
}

/** What this phone has, and anything that stops or slows the game on it. */
@Composable
private fun DeviceCheckCard(report: DeviceReport?) {
    Text(stringResource(R.string.device_title), style = MaterialTheme.typography.titleMedium)
    if (report == null) {
        Text(stringResource(R.string.device_checking))
        return
    }
    Text(
        stringResource(
            R.string.device_gles,
            report.glesVersion ?: stringResource(R.string.device_gles_none),
            report.glesRenderer.orEmpty(),
        ),
    )
    Text(stringResource(R.string.device_ram, report.totalRamMb))
    if (report.problems.isEmpty()) {
        Text(stringResource(R.string.device_ok), color = MaterialTheme.colorScheme.primary)
    }
    report.problems.forEach { problem ->
        Text(
            stringResource(problem.message()),
            color = if (problem.blocksGame) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.tertiary,
        )
    }
}

private fun DeviceProblem.message(): Int = when (this) {
    DeviceProblem.NOT_ARM64 -> R.string.game_unsupported_device
    DeviceProblem.NO_GLES3 -> R.string.device_no_gles3
    DeviceProblem.GLES_TOO_OLD -> R.string.device_gles_too_old
    DeviceProblem.GLES_UNTESTED -> R.string.device_gles_untested
    DeviceProblem.LOW_RAM -> R.string.device_low_ram
}

@Composable
private fun DownloadSection(
    state: DownloadState?,
    running: Boolean,
    onStart: () -> Unit,
    onCancel: () -> Unit,
) {
    Text(stringResource(R.string.download_title), style = MaterialTheme.typography.titleMedium)
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Button(enabled = !running, onClick = onStart) {
            Text(stringResource(if (state is DownloadState.Failed) R.string.download_retry else R.string.game_download))
        }
        if (running) {
            OutlinedButton(onClick = onCancel) { Text(stringResource(R.string.download_cancel)) }
        }
    }

    when (state) {
        null -> Text(stringResource(R.string.download_hint), style = MaterialTheme.typography.bodySmall)
        DownloadState.Checking -> {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            Text(stringResource(R.string.download_checking))
        }
        is DownloadState.InProgress -> {
            val fraction = if (state.totalBytes > 0) state.downloadedBytes.toFloat() / state.totalBytes else 0f
            LinearProgressIndicator(progress = { fraction }, modifier = Modifier.fillMaxWidth())
            Text(
                stringResource(
                    R.string.download_progress,
                    state.downloadedBytes / MB,
                    state.totalBytes / MB,
                    (fraction * 100).toInt(),
                ),
            )
            Text(
                stringResource(R.string.download_files, state.fileIndex, state.fileCount),
                style = MaterialTheme.typography.bodySmall,
            )
        }
        DownloadState.Verifying -> {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            Text(stringResource(R.string.download_verifying))
        }
        DownloadState.Completed ->
            Text(stringResource(R.string.game_download_completed), color = MaterialTheme.colorScheme.primary)
        DownloadState.WaitingForWifi -> Text(stringResource(R.string.download_waiting_wifi))
        is DownloadState.NotEnoughSpace -> Text(
            stringResource(R.string.download_no_space, state.neededBytes / MB, state.freeBytes / MB),
            color = MaterialTheme.colorScheme.error,
        )
        is DownloadState.Failed -> Text(
            stringResource(
                when (state.reason) {
                    DownloadState.FailureReason.NETWORK -> R.string.download_failed_network
                    DownloadState.FailureReason.CORRUPTED -> R.string.download_failed_corrupted
                    DownloadState.FailureReason.SERVER -> R.string.download_failed_server
                    DownloadState.FailureReason.STORAGE -> R.string.download_failed_storage
                },
            ),
            color = MaterialTheme.colorScheme.error,
        )
        is DownloadState.Unavailable -> Text(stringResource(R.string.game_download_unavailable))
    }
}

private const val MB = 1024L * 1024L
