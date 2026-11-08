package io.github.hypocketapp.launcher.ui.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.hypocketapp.core.auth.SecureStorageCheck
import io.github.hypocketapp.launcher.R
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Checks that the sign-in can be saved encrypted with the Android Keystore. */
@Composable
fun SecureStorageSection() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var running by remember { mutableStateOf(false) }
    var result by remember { mutableStateOf<SecureStorageCheck.Result?>(null) }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.storage_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.storage_description), style = MaterialTheme.typography.bodySmall)
        OutlinedButton(enabled = !running, onClick = {
            running = true
            scope.launch {
                result = withContext(Dispatchers.IO) { SecureStorageCheck.run(context.noBackupFilesDir) }
                running = false
            }
        }) { Text(stringResource(R.string.probe_run)) }
        result?.let {
            Text(
                stringResource(
                    when (it) {
                        SecureStorageCheck.Result.PASSED -> R.string.storage_passed
                        SecureStorageCheck.Result.READ_BACK_FAILED -> R.string.storage_read_back_failed
                        SecureStorageCheck.Result.PLAIN_TEXT_FOUND -> R.string.storage_plain_text
                        SecureStorageCheck.Result.ERROR -> R.string.storage_error
                    },
                ),
                color = if (it == SecureStorageCheck.Result.PASSED) {
                    MaterialTheme.colorScheme.primary
                } else {
                    MaterialTheme.colorScheme.error
                },
            )
        }
    }
}
