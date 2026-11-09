package io.github.hypocketapp.launcher

import android.app.Application
import io.github.hypocketapp.launcher.diagnostics.ExitDiagnostics

class HyPocketApplication : Application() {
    lateinit var container: AppContainer
        private set

    override fun onCreate() {
        super.onCreate()
        ExitDiagnostics.install(this)
        container = AppContainer(this)
    }
}
