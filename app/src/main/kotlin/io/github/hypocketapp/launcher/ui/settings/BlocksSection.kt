package io.github.hypocketapp.launcher.ui.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.hypocketapp.launcher.R
import io.github.hypocketapp.launcher.ui.blocks.BlocksActivity

/** Opens HyPocket Blocks, the test game. */
@Composable
fun BlocksSection() {
    val context = LocalContext.current
    var heavy by rememberSaveable { mutableStateOf(false) }
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.blocks_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.blocks_description), style = MaterialTheme.typography.bodySmall)
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(stringResource(R.string.blocks_heavy), modifier = Modifier.weight(1f))
            Switch(checked = heavy, onCheckedChange = { heavy = it })
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { BlocksActivity.start(context, heavy) }) { Text(stringResource(R.string.blocks_open)) }
            OutlinedButton(onClick = { BlocksActivity.startBenchmark(context) }) {
                Text(stringResource(R.string.blocks_benchmark))
            }
        }
        Text(stringResource(R.string.blocks_benchmark_description), style = MaterialTheme.typography.bodySmall)
    }
}
