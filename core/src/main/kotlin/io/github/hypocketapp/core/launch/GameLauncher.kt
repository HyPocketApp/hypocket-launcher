package io.github.hypocketapp.core.launch

import io.github.hypocketapp.core.account.VerifiedAccount
import io.github.hypocketapp.engine.Engine

/**
 * Starts the game. It takes a [VerifiedAccount], so the game can only be
 * started after the account check in this module has succeeded.
 */
class GameLauncher(
    private val isSupportedDevice: () -> Boolean = Engine::isSupportedDevice,
) {
    fun launch(account: VerifiedAccount): LaunchResult {
        if (!isSupportedDevice()) return LaunchResult.UnsupportedDevice
        // The engine (CPU and graphics translation) is not ready yet. Passing
        // the official session to the game client will also happen here.
        return LaunchResult.EngineNotReady
    }
}

sealed interface LaunchResult {
    data object Started : LaunchResult
    data object UnsupportedDevice : LaunchResult
    data object EngineNotReady : LaunchResult
}
