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
import io.github.hypocketapp.engine.ProbeReport
import io.github.hypocketapp.engine.ProbeStatus
import io.github.hypocketapp.engine.TranslatorProbe
import io.github.hypocketapp.launcher.R
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Runs the Box64 proof of concept and shows a report the tester can copy. */
@Composable
fun TranslatorTestSection() {
    val context = LocalContext.current
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    var running by remember { mutableStateOf(false) }
    var report by remember { mutableStateOf<ProbeReport?>(null) }
    var error by remember { mutableStateOf<String?>(null) }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.probe_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.probe_description), style = MaterialTheme.typography.bodySmall)

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                enabled = !running,
                onClick = {
                    running = true
                    error = null
                    scope.launch {
                        try {
                            report = withContext(Dispatchers.IO) { TranslatorProbe(context).run() }
                        } catch (e: Exception) {
                            error = e.toString()
                        } finally {
                            running = false
                        }
                    }
                },
            ) { Text(stringResource(R.string.probe_run)) }

            val current = report
            if (current != null || error != null) {
                OutlinedButton(onClick = {
                    val text = current?.toShareText() ?: error.orEmpty()
                    scope.launch {
                        clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("HyPocket report", text)))
                    }
                }) { Text(stringResource(R.string.probe_copy)) }
            }
        }

        if (running) CircularProgressIndicator()

        error?.let { Text(it, color = MaterialTheme.colorScheme.error) }

        report?.let { current ->
            Text(stringResource(current.status.label()), style = MaterialTheme.typography.titleSmall)
            Text(stringResource(R.string.probe_native_time, current.nativeWorkloadMillis))
            current.translatedWorkloadMillis?.let {
                Text(stringResource(R.string.probe_translated_time, it))
            }
            current.peakMemoryKb?.let {
                Text(stringResource(R.string.probe_peak_memory, it / 1024))
            }
            if (current.output.isNotBlank()) {
                SelectionContainer {
                    Text(
                        current.output,
                        style = MaterialTheme.typography.bodySmall,
                        fontFamily = FontFamily.Monospace,
                    )
                }
            }
        }
    }
}

private fun ProbeStatus.label(): Int = when (this) {
    ProbeStatus.PASSED -> R.string.probe_status_passed
    ProbeStatus.WRONG_RESULT -> R.string.probe_status_wrong_result
    ProbeStatus.FAILED -> R.string.probe_status_failed
    ProbeStatus.TIMED_OUT -> R.string.probe_status_timed_out
    ProbeStatus.TRANSLATOR_NOT_AVAILABLE -> R.string.probe_status_not_available
}

/** Plain-text report, in English, to paste into an issue. */
private fun ProbeReport.toShareText(): String = buildString {
    appendLine("HyPocket translator probe")
    appendLine("Device: ${android.os.Build.MANUFACTURER} ${android.os.Build.MODEL}")
    appendLine("Android: ${android.os.Build.VERSION.RELEASE} (API ${android.os.Build.VERSION.SDK_INT})")
    appendLine("SoC: ${if (android.os.Build.VERSION.SDK_INT >= 31) android.os.Build.SOC_MODEL else "unknown"}")
    appendLine("Status: $status")
    appendLine("Native: $nativeWorkloadMillis ms, checksum $nativeChecksum")
    appendLine("Translated: ${translatedWorkloadMillis ?: "-"} ms, checksum ${translatedChecksum ?: "-"}")
    appendLine("Total with startup: ${totalMillis ?: "-"} ms")
    appendLine("Peak memory: ${peakMemoryKb ?: "-"} kB")
    appendLine("Exit code: ${exitCode ?: "-"}")
    appendLine("--- output ---")
    append(output)
}
