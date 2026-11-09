package io.github.hypocketapp.launcher.ui.settings

import android.content.ClipData
import android.content.Intent
import android.text.format.DateFormat
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import io.github.hypocketapp.engine.RunLogs
import io.github.hypocketapp.launcher.R
import kotlinx.coroutines.launch
import java.util.Date

/**
 * The reports games and benchmarks saved on the phone while they ran, newest
 * first, including runs that were cut off before their end.
 */
@Composable
fun SavedReportsSection() {
    val context = LocalContext.current
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    val logs = remember { RunLogs(context) }
    var refresh by remember { mutableIntStateOf(0) }
    var showAll by remember { mutableStateOf(false) }
    var open by remember { mutableStateOf<String?>(null) }
    val entries = remember(refresh) { logs.list() }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.reports_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.reports_description), style = MaterialTheme.typography.bodySmall)
        if (entries.isEmpty()) {
            Text(stringResource(R.string.reports_none), style = MaterialTheme.typography.bodySmall)
        }
        entries.take(if (showAll) entries.size else SHOWN).forEach { entry ->
            val name = entry.file.name
            val date = DateFormat.getMediumDateFormat(context).format(Date(entry.startedAt)) + " " +
                DateFormat.getTimeFormat(context).format(Date(entry.startedAt))
            val state = stringResource(if (entry.finished) R.string.reports_finished else R.string.reports_interrupted)
            Text(
                stringResource(R.string.reports_item, date, entry.kind, state, (entry.sizeBytes + 1023) / 1024),
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.fillMaxWidth().clickable { open = if (open == name) null else name }.padding(vertical = 4.dp),
            )
            if (open == name) {
                val text = remember(name, refresh) { entry.read() }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = {
                        scope.launch {
                            clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("HyPocket report", text.forSharing())))
                        }
                    }) { Text(stringResource(R.string.reports_copy)) }
                    OutlinedButton(onClick = {
                        val send = Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text.forSharing())
                        context.startActivity(Intent.createChooser(send, null))
                    }) { Text(stringResource(R.string.reports_share)) }
                }
                SelectionContainer {
                    Text(
                        text.take(PREVIEW_CHARS) + if (text.length > PREVIEW_CHARS) "\n…" else "",
                        fontFamily = FontFamily.Monospace,
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TextButton(onClick = { refresh++ }) { Text(stringResource(R.string.reports_refresh)) }
            if (entries.size > SHOWN) {
                TextButton(onClick = { showAll = !showAll }) {
                    Text(stringResource(if (showAll) R.string.reports_show_less else R.string.reports_show_all, entries.size))
                }
            }
            if (entries.isNotEmpty()) {
                TextButton(onClick = {
                    logs.deleteAll()
                    open = null
                    refresh++
                }) { Text(stringResource(R.string.reports_delete_all)) }
            }
        }
    }
}

private const val SHOWN = 5
private const val PREVIEW_CHARS = 4000

/** The clipboard and the share sheet refuse very large texts: keep the start and the end. */
private const val MAX_SHARE_CHARS = 300_000

private fun String.forSharing(): String =
    if (length <= MAX_SHARE_CHARS) this
    else take(MAX_SHARE_CHARS / 2) + "\n[… middle of the report left out …]\n" + takeLast(MAX_SHARE_CHARS / 2)
