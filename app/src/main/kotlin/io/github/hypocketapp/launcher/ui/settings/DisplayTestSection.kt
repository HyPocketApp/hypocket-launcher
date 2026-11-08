package io.github.hypocketapp.launcher.ui.settings

import android.content.ClipData
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import io.github.hypocketapp.engine.DisplayProbe
import io.github.hypocketapp.engine.DisplayReport
import io.github.hypocketapp.engine.DisplayStatus
import io.github.hypocketapp.engine.DisplayTest
import io.github.hypocketapp.launcher.R
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Opens a full-screen on-screen test. */
@Composable
fun DisplayTestSection(test: DisplayTest) {
    var open by remember { mutableStateOf(false) }
    val (title, description, button) = when (test) {
        DisplayTest.SCENE -> Triple(R.string.display_title, R.string.display_description, R.string.display_open)
        DisplayTest.PERFORMANCE -> Triple(R.string.perf_title, R.string.perf_description, R.string.perf_open)
    }
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(description), style = MaterialTheme.typography.bodySmall)
        Button(onClick = { open = true }) { Text(stringResource(button)) }
    }
    if (open) DisplayTestDialog(test, onClose = { open = false })
}

/** The phone's full screen size in pixels, including system bars. */
private fun screenSize(context: android.content.Context): Pair<Int, Int> {
    if (android.os.Build.VERSION.SDK_INT >= 30) {
        val bounds = context.getSystemService(android.view.WindowManager::class.java).maximumWindowMetrics.bounds
        return bounds.width() to bounds.height()
    }
    val metrics = context.resources.displayMetrics
    return metrics.widthPixels to metrics.heightPixels
}

@Composable
private fun DisplayTestDialog(test: DisplayTest, onClose: () -> Unit) {
    val context = LocalContext.current
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    val probe = remember { DisplayProbe(context) }
    var view by remember { mutableStateOf<FrameSurfaceView?>(null) }
    var report by remember { mutableStateOf<DisplayReport?>(null) }
    var shownFps by remember { mutableStateOf(0.0) }
    var shownTotal by remember { mutableIntStateOf(0) }
    var startedAt by remember { mutableLongStateOf(0L) }

    LaunchedEffect(Unit) {
        startedAt = System.nanoTime()
        val (width, height) = screenSize(context)
        report = withContext(Dispatchers.IO) { probe.run(test, width, height, seconds = 15) }
    }
    // Update the "shown" frame rate once a second while the test runs.
    LaunchedEffect(view) {
        var previous = 0
        while (report == null) {
            delay(1000)
            val now = view?.shownFrames ?: 0
            shownFps = (now - previous).toDouble()
            previous = now
            shownTotal = now
        }
        shownTotal = view?.shownFrames ?: shownTotal
    }

    Dialog(onDismissRequest = onClose, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Box(Modifier.fillMaxSize().background(Color.Black)) {
            AndroidView(
                factory = { FrameSurfaceView(it, probe.frameFile).also { created -> view = created } },
                modifier = Modifier.fillMaxSize(),
            )
            Column(
                modifier = Modifier.align(Alignment.TopStart).padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                val current = report
                if (current == null) {
                    Text(
                        stringResource(if (test == DisplayTest.PERFORMANCE) R.string.perf_running else R.string.display_running),
                        color = Color.White,
                    )
                    Text(stringResource(R.string.display_shown_fps, shownFps), color = Color.White)
                } else {
                    Text(stringResource(current.status.label()), color = Color.White)
                    current.guestFps?.let { Text(stringResource(R.string.display_guest_fps, it), color = Color.White) }
                    current.steps.forEach { Text(it, color = Color.White, style = MaterialTheme.typography.bodySmall) }
                    if (current.drawMillis != null && current.readBackMillis != null) {
                        Text(
                            stringResource(R.string.display_per_frame, current.drawMillis!!, current.readBackMillis!!),
                            color = Color.White,
                        )
                    }
                    val seconds = (System.nanoTime() - startedAt) / 1e9
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = {
                            val text = current.toShareText(shownTotal, seconds)
                            scope.launch {
                                clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("HyPocket report", text)))
                            }
                        }) { Text(stringResource(R.string.probe_copy)) }
                        OutlinedButton(onClick = onClose) { Text(stringResource(R.string.display_close), color = Color.White) }
                    }
                }
            }
        }
    }
}

private fun DisplayStatus.label(): Int = when (this) {
    DisplayStatus.PASSED -> R.string.display_status_passed
    DisplayStatus.FAILED -> R.string.probe_status_failed
    DisplayStatus.TIMED_OUT -> R.string.probe_status_timed_out
    DisplayStatus.BLOCKED_SYSCALL -> R.string.probe_status_blocked_syscall
    DisplayStatus.SERVER_FAILED -> R.string.gfx_status_server_failed
    DisplayStatus.NOT_AVAILABLE -> R.string.probe_status_not_available
}

/** Plain-text report, in English, to paste into an issue. */
private fun DisplayReport.toShareText(shownFrames: Int, seconds: Double): String = buildString {
    appendLine("HyPocket display probe: $test")
    appendLine("Device: ${android.os.Build.MANUFACTURER} ${android.os.Build.MODEL}")
    appendLine("Android: ${android.os.Build.VERSION.RELEASE} (API ${android.os.Build.VERSION.SDK_INT})")
    appendLine("SoC: ${if (android.os.Build.VERSION.SDK_INT >= 31) android.os.Build.SOC_MODEL else "unknown"}")
    appendLine("Status: $status")
    appendLine("Renderer: ${renderer ?: "-"}")
    appendLine("Guest fps (640x360, draw + read back): ${guestFps ?: "-"}")
    appendLine("Per frame: draw ${drawMillis ?: "-"} ms, read back ${readBackMillis ?: "-"} ms")
    appendLine("Frames shown by the app: $shownFrames in %.1f s".format(seconds))
    steps.forEach { appendLine("Step: $it") }
    appendLine("Exit code: ${exitCode ?: "-"}")
    memory?.toLines()?.forEach { appendLine("Memory: $it") }
    appendLine("--- probe output ---")
    appendLine(output)
    appendLine("--- server output ---")
    append(serverOutput)
}
