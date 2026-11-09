package io.github.hypocketapp.launcher.ui.settings

import android.content.ClipData
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.text.selection.SelectionContainer
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
import io.github.hypocketapp.launcher.R
import io.github.hypocketapp.launcher.diagnostics.ExitDiagnostics
import kotlinx.coroutines.launch

/** Shows why the app's previous runs ended, so testers can report crashes. */
@Composable
fun LastExitSection() {
    val context = LocalContext.current
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    var report by remember { mutableStateOf<String?>(null) }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.exit_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.exit_description), style = MaterialTheme.typography.bodySmall)
        OutlinedButton(onClick = {
            val text = ExitDiagnostics.report(context)
            report = text
            scope.launch {
                clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("HyPocket report", text)))
            }
        }) { Text(stringResource(R.string.exit_copy)) }
        report?.let {
            SelectionContainer {
                Text(it, fontFamily = FontFamily.Monospace, style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}
