package io.github.hypocketapp.blocks.ui

import android.content.ClipData
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
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
import io.github.hypocketapp.blocks.BlocksBenchmark
import io.github.hypocketapp.engine.DirectDisplay
import io.github.hypocketapp.engine.RunLogs
import io.github.hypocketapp.blocks.R
import io.github.hypocketapp.engine.view.FrameSurfaceView
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Runs [BlocksBenchmark]: the heavy world with a fixed camera path, once per
 * set of options, then a table to compare them and a report to copy.
 */
@Composable
fun BenchmarkScreen(benchmark: BlocksBenchmark, screenWidth: Int, screenHeight: Int, versionLine: String, onClose: () -> Unit) {
    val directDisplay = remember { CompletableDeferred<DirectDisplay?>() }
    var phase by remember { mutableIntStateOf(0) }
    var results by remember { mutableStateOf<List<BlocksBenchmark.PhaseResult>?>(null) }
    val context = LocalContext.current

    LaunchedEffect(Unit) {
        val display = directDisplay.await()
        val log = RunLogs(context).start(
            "benchmark",
            "HyPocket Blocks benchmark\n" + deviceLines(versionLine),
        )
        results = try {
            withContext(Dispatchers.IO) {
                benchmark.run(screenWidth, screenHeight, display, log) { index -> phase = index }
                    .also { log.finish(it.toShareText(versionLine)) }
            }
        } finally {
            log.close()
        }
    }

    Box(Modifier.fillMaxSize().background(Color.Black)) {
        AndroidView(
            factory = {
                // The benchmark always uses direct display; the file is only
                // for the fallback when it is not available.
                FrameSurfaceView(it, File(context.cacheDir, "hypocket-frame.bin"), direct = true) { display ->
                    directDisplay.complete(display)
                }
            },
            modifier = Modifier.fillMaxSize(),
        )
        val done = results
        if (done == null) {
            val total = BlocksBenchmark.PHASES.size
            Column(Modifier.align(Alignment.TopStart).padding(16.dp)) {
                Text(
                    stringResource(R.string.bench_running, phase + 1, total, BlocksBenchmark.PHASES[phase].name),
                    color = Color.White,
                )
                Text(stringResource(R.string.bench_hint), color = Color.White.copy(alpha = 0.7f),
                    style = MaterialTheme.typography.bodySmall)
            }
        } else {
            BenchmarkResults(done, versionLine, onClose)
        }
    }
}

@Composable
private fun BenchmarkResults(results: List<BlocksBenchmark.PhaseResult>, versionLine: String, onClose: () -> Unit) {
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    Column(
        Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.88f)).padding(24.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Text(stringResource(R.string.bench_title), color = Color.White, style = MaterialTheme.typography.titleMedium)
        results.forEach { r ->
            Text(
                stringResource(
                    R.string.bench_row, r.phase.name, r.fps ?: 0.0, r.cpuMillis ?: 0.0, r.waitMillis ?: 0.0,
                    r.serverMillis ?: 0.0,
                ),
                color = Color.White,
                style = MaterialTheme.typography.bodySmall,
            )
        }
        Text(stringResource(R.string.bench_legend), color = Color.White.copy(alpha = 0.7f),
            style = MaterialTheme.typography.bodySmall)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = {
                scope.launch {
                    clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("HyPocket report", results.toShareText(versionLine))))
                }
            }) { Text(stringResource(R.string.blocks_copy_report)) }
            OutlinedButton(onClick = onClose) { Text(stringResource(R.string.blocks_close), color = Color.White) }
        }
    }
}

/** Plain-text report, in English, to paste into an issue. */
private fun List<BlocksBenchmark.PhaseResult>.toShareText(versionLine: String): String {
    val results = this
    return buildString {
        appendLine(
            "HyPocket Blocks benchmark (heavy mode, ${BlocksBenchmark.FRAMES} frames per phase, " +
                "no FPS or refresh-rate limit)",
        )
        append(deviceLines(versionLine))
        results.firstOrNull()?.report?.renderer?.let { appendLine("Renderer: $it") }
        appendLine("phase | fps | game cpu ms | wait ms | server ms/frame | server busy % | status")
        results.forEach { r ->
            appendLine(
                "%s | %s | %s | %s | %s | %s | %s".format(
                    r.phase.name, r.fps.fmt(), r.cpuMillis.fmt(2), r.waitMillis.fmt(2), r.serverMillis.fmt(2),
                    r.serverBusyPercent.fmt(0), r.status,
                ),
            )
        }
        results.forEach { r ->
            appendLine("--- ${r.phase.name}: game ---")
            r.report.output.lines().filter { it.startsWith("bench:") || it.startsWith("blocks: ") }
                .takeLast(6).forEach { appendLine(it) }
            appendLine("--- ${r.phase.name}: server ---")
            r.report.serverOutput.lines().filter { it.startsWith("hypocket-stats") || it.startsWith("hypocket-profile") }.forEach { appendLine(it) }
            r.report.memory?.toLines()?.forEach { appendLine("Memory: $it") }
        }
    }
}

private fun Double?.fmt(decimals: Int = 1): String = this?.let { "%.${decimals}f".format(it) } ?: "-"
