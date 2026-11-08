package io.github.hypocketapp.launcher.diagnostics

import io.github.hypocketapp.launcher.BuildConfig

/** "App: 0.1.0 (commit abcdef12)", so a test report can be matched to the code. */
fun appVersionLine(): String = "App: ${BuildConfig.VERSION_NAME} (commit ${BuildConfig.GIT_COMMIT})"
