package io.github.hypocketapp.launcher.ui

import androidx.annotation.StringRes
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AccountCircle
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import io.github.hypocketapp.launcher.AppContainer
import io.github.hypocketapp.launcher.R
import io.github.hypocketapp.launcher.ui.account.AccountScreen
import io.github.hypocketapp.launcher.ui.game.GameScreen
import io.github.hypocketapp.launcher.ui.settings.SettingsScreen

private enum class Tab(@param:StringRes val label: Int, val icon: ImageVector) {
    ACCOUNT(R.string.tab_account, Icons.Filled.AccountCircle),
    GAME(R.string.tab_game, Icons.Filled.PlayArrow),
    SETTINGS(R.string.tab_settings, Icons.Filled.Settings),
}

@Composable
fun HyPocketApp(container: AppContainer) {
    var current by rememberSaveable { mutableStateOf(Tab.ACCOUNT) }

    Scaffold(
        bottomBar = {
            NavigationBar {
                Tab.entries.forEach { tab ->
                    NavigationBarItem(
                        selected = tab == current,
                        onClick = { current = tab },
                        icon = { Icon(tab.icon, contentDescription = null) },
                        label = { Text(stringResource(tab.label)) },
                    )
                }
            }
        },
    ) { padding ->
        val modifier = Modifier.padding(padding)
        when (current) {
            Tab.ACCOUNT -> AccountScreen(container.accountSession, modifier)
            Tab.GAME -> GameScreen(container, modifier)
            Tab.SETTINGS -> SettingsScreen(container.settingsStore, modifier)
        }
    }
}
