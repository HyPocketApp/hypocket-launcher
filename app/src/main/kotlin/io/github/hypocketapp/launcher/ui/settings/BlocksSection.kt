package io.github.hypocketapp.launcher.ui.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.hypocketapp.launcher.R
import io.github.hypocketapp.launcher.ui.blocks.BlocksActivity

/** Opens HyPocket Blocks, the test game. */
@Composable
fun BlocksSection() {
    val context = LocalContext.current
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.blocks_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.blocks_description), style = MaterialTheme.typography.bodySmall)
        Button(onClick = { BlocksActivity.start(context) }) { Text(stringResource(R.string.blocks_open)) }
    }
}
