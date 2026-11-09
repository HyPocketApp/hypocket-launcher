package io.github.hypocketapp.core.launch

import io.github.hypocketapp.core.account.VerifiedAccount
import io.github.hypocketapp.engine.DeviceReport

/**
 * Starts the game. It takes a [VerifiedAccount], so the game can only be
 * started after the account check in this module has succeeded, and a
 * [DeviceReport], so it is not started on a phone that cannot run it.
 */
class GameLauncher {
    fun launch(account: VerifiedAccount, device: DeviceReport): LaunchResult {
        if (!device.canRun) return LaunchResult.UnsupportedDevice
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
