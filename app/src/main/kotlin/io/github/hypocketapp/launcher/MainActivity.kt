package io.github.hypocketapp.launcher

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import io.github.hypocketapp.launcher.ui.HyPocketApp
import io.github.hypocketapp.launcher.ui.theme.HyPocketTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val container = (application as HyPocketApplication).container
        setContent {
            HyPocketTheme {
                HyPocketApp(container)
            }
        }
    }
}
