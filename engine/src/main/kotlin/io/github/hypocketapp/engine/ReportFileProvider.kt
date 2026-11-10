package io.github.hypocketapp.engine

import androidx.core.content.FileProvider

/**
 * Hands a saved report to the app the user shares it with, as a file
 * (see [RunLogs.shareIntent]). A class of its own so that it does not clash
 * with a FileProvider the app may declare.
 */
class ReportFileProvider : FileProvider()
