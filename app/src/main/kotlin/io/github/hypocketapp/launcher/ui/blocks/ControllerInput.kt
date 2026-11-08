package io.github.hypocketapp.launcher.ui.blocks

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.compose.runtime.mutableStateOf
import io.github.hypocketapp.engine.BlocksSession
import kotlin.math.abs
import kotlin.math.sign

/**
 * Game controllers and keyboards (Bluetooth or USB) for HyPocket Blocks.
 *
 * Controller: left stick or d-pad walks, right stick looks, A jumps, X / R1 /
 * right trigger breaks, B / L1 / left trigger places, Start exits.
 * Keyboard: WASD walks, arrow keys look, Space jumps, Q breaks, E places,
 * Esc exits.
 */
class ControllerInput(private val session: BlocksSession, private val onExit: () -> Unit) {

    /** True once a controller or keyboard was used, to show its hint. */
    val used = mutableStateOf(false)

    /** Names of the devices used, for the report. */
    val deviceNames = linkedSetOf<String>()

    private var stickMove = 0f to 0f
    private var keyMove = 0f to 0f
    private var lastMove = 0f to 0f
    private var stickLook = 0f to 0f
    private var keyLook = 0f to 0f
    private var rightTrigger = false
    private var leftTrigger = false
    private val keysDown = mutableSetOf<Int>()

    /** The last d-pad keys came from a controller (walk) rather than a keyboard (look). */
    private var dpadWalks = true

    /** Joystick motion (sticks, d-pad as a hat, analog triggers). */
    fun onMotion(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK ||
            event.action != MotionEvent.ACTION_MOVE
        ) {
            return false
        }
        noteDevice(event.device)
        val device = event.device
        var x = axis(event, device, MotionEvent.AXIS_X)
        var y = axis(event, device, MotionEvent.AXIS_Y)
        if (x == 0f && y == 0f) {
            x = event.getAxisValue(MotionEvent.AXIS_HAT_X)
            y = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
        }
        stickMove = x to -y
        // Most controllers put the right stick on Z/RZ, some on RX/RY.
        var rx = axis(event, device, MotionEvent.AXIS_Z)
        var ry = axis(event, device, MotionEvent.AXIS_RZ)
        if (rx == 0f && ry == 0f) {
            rx = axis(event, device, MotionEvent.AXIS_RX)
            ry = axis(event, device, MotionEvent.AXIS_RY)
        }
        // Squared response: fine aim near the centre, fast turns at the edge.
        stickLook = rx * abs(rx) * LOOK_YAW_RATE to -ry * abs(ry) * LOOK_PITCH_RATE
        val right = maxOf(event.getAxisValue(MotionEvent.AXIS_RTRIGGER), event.getAxisValue(MotionEvent.AXIS_GAS))
        val left = maxOf(event.getAxisValue(MotionEvent.AXIS_LTRIGGER), event.getAxisValue(MotionEvent.AXIS_BRAKE))
        if (right > TRIGGER_ON && !rightTrigger) session.breakBlock()
        if (left > TRIGGER_ON && !leftTrigger) session.placeBlock()
        rightTrigger = right > TRIGGER_ON
        leftTrigger = left > TRIGGER_ON
        update()
        return true
    }

    /** Controller buttons and keyboard keys. Returns true when handled. */
    fun onKey(event: KeyEvent): Boolean {
        val code = event.keyCode
        if (!isGameKey(code, event)) return false
        noteDevice(event.device)
        val down = event.action == KeyEvent.ACTION_DOWN
        if (down && event.repeatCount > 0) return true
        if (down) keysDown += code else keysDown -= code
        if (code in DPAD_KEYS) dpadWalks = !event.isFromKeyboard()
        if (down) {
            when (code) {
                KeyEvent.KEYCODE_BUTTON_X, KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_R2,
                KeyEvent.KEYCODE_Q,
                -> session.breakBlock()
                KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_L2,
                KeyEvent.KEYCODE_E,
                -> session.placeBlock()
                KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_ESCAPE -> onExit()
            }
        }
        session.setJump(KeyEvent.KEYCODE_BUTTON_A in keysDown || KeyEvent.KEYCODE_SPACE in keysDown)
        fun axisOf(negative: Set<Int>, positive: Set<Int>) =
            (if (keysDown.any { it in positive }) 1f else 0f) - (if (keysDown.any { it in negative }) 1f else 0f)
        // On a controller the d-pad walks; on a keyboard the arrow keys look.
        val left = setOf(KeyEvent.KEYCODE_DPAD_LEFT)
        val right = setOf(KeyEvent.KEYCODE_DPAD_RIGHT)
        val up = setOf(KeyEvent.KEYCODE_DPAD_UP)
        val downKeys = setOf(KeyEvent.KEYCODE_DPAD_DOWN)
        keyMove = if (dpadWalks) {
            axisOf(LEFT_KEYS + left, RIGHT_KEYS + right) to axisOf(BACK_KEYS + downKeys, FORWARD_KEYS + up)
        } else {
            axisOf(LEFT_KEYS, RIGHT_KEYS) to axisOf(BACK_KEYS, FORWARD_KEYS)
        }
        keyLook = if (dpadWalks) {
            0f to 0f
        } else {
            axisOf(left, right) * KEY_LOOK_RATE to axisOf(downKeys, up) * KEY_LOOK_RATE * 0.7f
        }
        update()
        return true
    }

    private fun update() {
        val move = if (stickMove.first != 0f || stickMove.second != 0f) stickMove else keyMove
        // Only send changes, so touch controls keep working alongside.
        if (move != lastMove) {
            session.setMove(move.first, move.second)
            lastMove = move
        }
        session.setLookRate(stickLook.first + keyLook.first, stickLook.second + keyLook.second)
    }

    private fun noteDevice(device: InputDevice?) {
        used.value = true
        device?.name?.let { if (deviceNames.size < 4) deviceNames += it }
    }

    private fun KeyEvent.isFromKeyboard(): Boolean =
        source and InputDevice.SOURCE_KEYBOARD == InputDevice.SOURCE_KEYBOARD &&
            source and InputDevice.SOURCE_GAMEPAD != InputDevice.SOURCE_GAMEPAD &&
            device?.keyboardType == InputDevice.KEYBOARD_TYPE_ALPHABETIC

    private fun isGameKey(code: Int, event: KeyEvent): Boolean {
        if (KeyEvent.isGamepadButton(code)) return true
        // The d-pad of a controller, or the arrow keys of a keyboard.
        val fromController = event.source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
            event.source and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD
        if (code in DPAD_KEYS) return fromController || event.isFromKeyboard()
        return event.isFromKeyboard() && code in KEYBOARD_KEYS
    }

    /** An axis value with the device's dead zone removed. */
    private fun axis(event: MotionEvent, device: InputDevice?, axis: Int): Float {
        val range = device?.getMotionRange(axis, event.source) ?: return 0f
        val value = event.getAxisValue(axis)
        val flat = maxOf(range.flat, DEAD_ZONE)
        return if (abs(value) <= flat) 0f else sign(value) * (abs(value) - flat) / (1f - flat)
    }

    private companion object {
        const val DEAD_ZONE = 0.12f
        const val TRIGGER_ON = 0.5f
        const val LOOK_YAW_RATE = 3.2f // radians per second at full stick
        const val LOOK_PITCH_RATE = 2.2f
        const val KEY_LOOK_RATE = 2.0f

        val DPAD_KEYS = setOf(
            KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_DPAD_LEFT,
            KeyEvent.KEYCODE_DPAD_RIGHT,
        )
        val KEYBOARD_KEYS = setOf(
            KeyEvent.KEYCODE_W, KeyEvent.KEYCODE_A, KeyEvent.KEYCODE_S, KeyEvent.KEYCODE_D, KeyEvent.KEYCODE_SPACE,
            KeyEvent.KEYCODE_Q, KeyEvent.KEYCODE_E, KeyEvent.KEYCODE_ESCAPE,
        )
        val FORWARD_KEYS = setOf(KeyEvent.KEYCODE_W)
        val BACK_KEYS = setOf(KeyEvent.KEYCODE_S)
        val LEFT_KEYS = setOf(KeyEvent.KEYCODE_A)
        val RIGHT_KEYS = setOf(KeyEvent.KEYCODE_D)
    }
}
