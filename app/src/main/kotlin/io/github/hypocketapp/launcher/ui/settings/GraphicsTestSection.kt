package io.github.hypocketapp.launcher.ui.settings

import android.content.ClipData
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import io.github.hypocketapp.engine.GraphicsProbe
import io.github.hypocketapp.engine.GraphicsReport
import io.github.hypocketapp.engine.GraphicsStatus
import io.github.hypocketapp.engine.GraphicsTest
import io.github.hypocketapp.launcher.diagnostics.appVersionLine
import io.github.hypocketapp.launcher.R
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Runs the graphics proof of concept and shows a report the tester can copy. */
@Composable
fun GraphicsTestSection() {
    val context = LocalContext.current
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    var running by remember { mutableStateOf(false) }
    var reports by remember { mutableStateOf<List<GraphicsReport>>(emptyList()) }
    var error by remember { mutableStateOf<String?>(null) }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.gfx_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.gfx_description), style = MaterialTheme.typography.bodySmall)

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                enabled = !running,
                onClick = {
                    running = true
                    error = null
                    scope.launch {
                        try {
                            reports = emptyList()
                            val probe = GraphicsProbe(context)
                            // One after the other: the server serves one client and exits.
                            for (test in GraphicsTest.entries) {
                                val result = withContext(Dispatchers.IO) { probe.run(test) }
                                reports = reports + result
                            }
                        } catch (e: Exception) {
                            error = e.toString()
                        } finally {
                            running = false
                        }
                    }
                },
            ) { Text(stringResource(R.string.probe_run)) }

            if (reports.isNotEmpty() || error != null) {
                OutlinedButton(onClick = {
                    val text = (reports.map { it.toShareText() } + listOfNotNull(error)).joinToString("\n\n")
                    scope.launch {
                        clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("HyPocket report", text)))
                    }
                }) { Text(stringResource(R.string.probe_copy)) }
            }
        }

        if (running) CircularProgressIndicator()
        error?.let { Text(it, color = MaterialTheme.colorScheme.error) }

        reports.forEach { current ->
            Text(stringResource(current.test.label()), style = MaterialTheme.typography.titleSmall)
            Text(stringResource(current.status.label()))
            current.renderer?.let { Text(stringResource(R.string.gfx_renderer, it)) }
            current.highestCoreVersion?.let { Text(stringResource(R.string.gfx_core_version, it)) }
            current.frameMillis?.let { Text(stringResource(R.string.gfx_frames, it)) }
            val output = listOf(current.output, current.serverOutput).filter { it.isNotBlank() }.joinToString("\n")
            if (output.isNotBlank()) {
                SelectionContainer {
                    Text(output, style = MaterialTheme.typography.bodySmall, fontFamily = FontFamily.Monospace)
                }
            }
        }
    }
}

private fun GraphicsTest.label(): Int = when (this) {
    GraphicsTest.GLES -> R.string.gfx_test_gles
    GraphicsTest.DESKTOP_CORE -> R.string.gfx_test_desktop
    GraphicsTest.DESKTOP_CORE_FORCED -> R.string.gfx_test_desktop_forced
}

private fun GraphicsStatus.label(): Int = when (this) {
    GraphicsStatus.PASSED -> R.string.gfx_status_passed
    GraphicsStatus.WRONG_RESULT -> R.string.gfx_status_wrong_result
    GraphicsStatus.BELOW_REQUIRED_VERSION -> R.string.gfx_status_below_required
    GraphicsStatus.FAILED -> R.string.probe_status_failed
    GraphicsStatus.TIMED_OUT -> R.string.probe_status_timed_out
    GraphicsStatus.BLOCKED_SYSCALL -> R.string.probe_status_blocked_syscall
    GraphicsStatus.SERVER_FAILED -> R.string.gfx_status_server_failed
    GraphicsStatus.NOT_AVAILABLE -> R.string.probe_status_not_available
}

/** Plain-text report, in English, to paste into an issue. */
private fun GraphicsReport.toShareText(): String = buildString {
    appendLine("HyPocket graphics probe: $test")
    appendLine("Device: ${android.os.Build.MANUFACTURER} ${android.os.Build.MODEL}")
    appendLine(appVersionLine())
    appendLine("Android: ${android.os.Build.VERSION.RELEASE} (API ${android.os.Build.VERSION.SDK_INT})")
    appendLine("SoC: ${if (android.os.Build.VERSION.SDK_INT >= 31) android.os.Build.SOC_MODEL else "unknown"}")
    appendLine("Status: $status")
    appendLine("Renderer: ${renderer ?: "-"}")
    appendLine("Version: ${version ?: "-"}")
    if (test != GraphicsTest.GLES) appendLine("Highest core profile: ${highestCoreVersion ?: "-"}")
    appendLine("120 frames: ${frameMillis ?: "-"} ms")
    appendLine("Total with startup: ${totalMillis ?: "-"} ms")
    appendLine("Exit code: ${exitCode ?: "-"}")
    appendLine("--- probe output ---")
    appendLine(output)
    appendLine("--- server output ---")
    append(serverOutput)
}
