package io.github.hypocketapp.launcher

import android.app.Application

class HyPocketApplication : Application() {
    lateinit var container: AppContainer
        private set

    override fun onCreate() {
        super.onCreate()
        container = AppContainer(this)
    }
}
