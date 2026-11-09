package io.github.hypocketapp.blocks.ui

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.compose.runtime.mutableStateOf
import io.github.hypocketapp.blocks.BlocksSession
import kotlin.math.abs
import kotlin.math.roundToInt
import kotlin.math.sign

/**
 * Game controllers and keyboards (Bluetooth or USB) for HyPocket Blocks.
 *
 * Controller: left stick or d-pad walks, right stick looks, A jumps, X / R1 /
 * right trigger breaks, B / L1 / left trigger places, Y picks the next
 * block, Select opens the inventory, Start exits.
 * Keyboard: WASD walks, arrow keys look, Space jumps, Q breaks, E places,
 * 1-9 pick a place in the hotbar, I or Tab opens the inventory, Esc exits.
 * In the inventory the d-pad or arrow keys move, A or Enter puts the block in
 * the hotbar, and B, Select, I, Tab or Esc close it.
 */
class ControllerInput(
    private val session: BlocksSession,
    /** 1 is the default camera speed (the launcher's sensitivity setting). */
    private val sensitivity: Float,
    private val selection: BlockSelection,
    private val inventory: Inventory,
    private val onExit: () -> Unit,
) {

    /** True once a controller or keyboard was used, to show its hint. */
    val used = mutableStateOf(false)

    /** Names of the devices used, for the report. */
    val deviceNames = linkedSetOf<String>()

    /** Events received from controllers and keyboards, for the report. */
    var motionEvents = 0
        private set
    var keyEvents = 0
        private set

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

    /** The d-pad as a hat, last seen, so the inventory moves once per press. */
    private var hat = 0 to 0

    /** Forget held keys and sticks: the inventory opened or closed in between. */
    fun reset() {
        keysDown.clear()
        stickMove = 0f to 0f
        keyMove = 0f to 0f
        stickLook = 0f to 0f
        keyLook = 0f to 0f
        lastMove = 0f to 0f
        session.setMove(0f, 0f)
        session.setLookRate(0f, 0f)
        session.setJump(false)
    }

    /** Joystick motion (sticks, d-pad as a hat, analog triggers). */
    fun onMotion(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK ||
            event.action != MotionEvent.ACTION_MOVE
        ) {
            return false
        }
        noteDevice(event.device)
        motionEvents++
        if (inventory.open.value) {
            // Only the d-pad, when the controller reports it as a hat, moves the cursor.
            val now = event.getAxisValue(MotionEvent.AXIS_HAT_X).roundToInt() to
                event.getAxisValue(MotionEvent.AXIS_HAT_Y).roundToInt()
            if (now != hat && (now.first != 0 || now.second != 0)) inventory.move(now.first, now.second)
            hat = now
            return true
        }
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
        stickLook = rx * abs(rx) * LOOK_YAW_RATE * sensitivity to -ry * abs(ry) * LOOK_PITCH_RATE * sensitivity
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
        keyEvents++
        val down = event.action == KeyEvent.ACTION_DOWN
        if (down && event.repeatCount > 0) return true
        if (inventory.open.value) {
            if (down) onInventoryKey(code)
            return true
        }
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
                KeyEvent.KEYCODE_BUTTON_Y -> selection.next()
                in INVENTORY_KEYS -> {
                    inventory.show()
                    return true
                }
                KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_ESCAPE -> onExit()
                in NUMBER_KEYS -> selection.select(NUMBER_KEYS.indexOf(code))
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
            axisOf(left, right) * KEY_LOOK_RATE * sensitivity to axisOf(downKeys, up) * KEY_LOOK_RATE * 0.7f * sensitivity
        }
        update()
        return true
    }

    private fun onInventoryKey(code: Int) {
        when (code) {
            KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_A -> inventory.move(-1, 0)
            KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.KEYCODE_D -> inventory.move(1, 0)
            KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_W -> inventory.move(0, -1)
            KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_S -> inventory.move(0, 1)
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_SPACE, KeyEvent.KEYCODE_E ->
                inventory.confirm(selection)
            KeyEvent.KEYCODE_BUTTON_L1 -> selection.previous()
            KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_Y -> selection.next()
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_ESCAPE, KeyEvent.KEYCODE_BUTTON_START -> inventory.close()
            in INVENTORY_KEYS -> inventory.close()
            in NUMBER_KEYS -> selection.select(NUMBER_KEYS.indexOf(code))
        }
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

    companion object {
        /**
         * The external controllers, keyboards and mice Android sees right now,
         * for the report: whether a controller is connected at all, and as what.
         */
        fun connectedDevices(): List<String> = InputDevice.getDeviceIds().toList().mapNotNull { id ->
            val device = InputDevice.getDevice(id) ?: return@mapNotNull null
            if (device.isVirtual) return@mapNotNull null
            if (android.os.Build.VERSION.SDK_INT >= 29 && !device.isExternal) return@mapNotNull null
            val kinds = buildList {
                if (device.supportsSource(InputDevice.SOURCE_GAMEPAD)) add("gamepad")
                if (device.supportsSource(InputDevice.SOURCE_JOYSTICK)) add("joystick")
                if (device.keyboardType == InputDevice.KEYBOARD_TYPE_ALPHABETIC) add("keyboard")
                if (device.supportsSource(InputDevice.SOURCE_MOUSE)) add("mouse")
                if (device.supportsSource(InputDevice.SOURCE_TOUCHSCREEN)) add("touchscreen")
            }
            if (kinds.isEmpty()) null else "${device.name} (${kinds.joinToString("/")})"
        }

        private const val DEAD_ZONE = 0.12f
        private const val TRIGGER_ON = 0.5f
        private const val LOOK_YAW_RATE = 4.0f // radians per second at full stick
        private const val LOOK_PITCH_RATE = 2.6f
        private const val KEY_LOOK_RATE = 2.0f

        private val DPAD_KEYS = setOf(
            KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_DPAD_LEFT,
            KeyEvent.KEYCODE_DPAD_RIGHT,
        )
        /** 1 to 9: the places of the hotbar. */
        private val NUMBER_KEYS = listOf(
            KeyEvent.KEYCODE_1, KeyEvent.KEYCODE_2, KeyEvent.KEYCODE_3, KeyEvent.KEYCODE_4, KeyEvent.KEYCODE_5,
            KeyEvent.KEYCODE_6, KeyEvent.KEYCODE_7, KeyEvent.KEYCODE_8, KeyEvent.KEYCODE_9,
        )
        private val INVENTORY_KEYS = setOf(KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_I, KeyEvent.KEYCODE_TAB)
        private val KEYBOARD_KEYS = setOf(
            KeyEvent.KEYCODE_W, KeyEvent.KEYCODE_A, KeyEvent.KEYCODE_S, KeyEvent.KEYCODE_D, KeyEvent.KEYCODE_SPACE,
            KeyEvent.KEYCODE_Q, KeyEvent.KEYCODE_E, KeyEvent.KEYCODE_ESCAPE, KeyEvent.KEYCODE_I, KeyEvent.KEYCODE_TAB,
            KeyEvent.KEYCODE_ENTER,
        ) + NUMBER_KEYS
        private val FORWARD_KEYS = setOf(KeyEvent.KEYCODE_W)
        private val BACK_KEYS = setOf(KeyEvent.KEYCODE_S)
        private val LEFT_KEYS = setOf(KeyEvent.KEYCODE_A)
        private val RIGHT_KEYS = setOf(KeyEvent.KEYCODE_D)
    }
}
