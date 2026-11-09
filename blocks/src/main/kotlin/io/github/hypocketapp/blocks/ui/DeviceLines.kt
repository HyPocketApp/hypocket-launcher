package io.github.hypocketapp.blocks.ui

import android.os.Build

/** The device lines every report starts with, in English, each ending in a newline. */
internal fun deviceLines(versionLine: String): String = buildString {
    appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL}")
    appendLine(versionLine)
    appendLine("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
    appendLine("SoC: ${if (Build.VERSION.SDK_INT >= 31) Build.SOC_MODEL else "unknown"}")
}
