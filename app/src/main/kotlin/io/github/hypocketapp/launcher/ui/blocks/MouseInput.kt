package io.github.hypocketapp.launcher.ui.blocks

import android.view.InputDevice
import android.view.MotionEvent
import android.view.View
import androidx.compose.runtime.mutableStateOf
import io.github.hypocketapp.engine.BlocksSession

/**
 * A mouse (Bluetooth, USB, or an adapter that passes one through) for
 * HyPocket Blocks, like in a PC game: a click captures the pointer, then
 * moving the mouse looks around, the left button breaks and the right button
 * places. Esc gives the pointer back.
 *
 * Captured events carry relative movement and reach only [view], so the
 * pointer neither leaves the game nor touches its controls.
 */
class MouseInput(private val session: BlocksSession) {

    /** True while the pointer is captured, to show the mouse hint. */
    val captured = mutableStateOf(false)

    /** Names of the mice used, for the report. */
    val deviceNames = linkedSetOf<String>()

    private var view: View? = null
    private var buttons = 0
    private var enabled = true

    /** The view that receives captured events; it must be able to take focus. */
    fun attach(target: View) {
        view = target
        target.isFocusable = true
        target.isFocusableInTouchMode = true
        target.setOnCapturedPointerListener { _, event -> onCaptured(event) }
    }

    /**
     * Mouse events before capture (they would otherwise act as touches, and a
     * right click as Back): a button press captures the pointer. Returns true
     * when the event was a mouse's and was used.
     */
    fun onUncaptured(event: MotionEvent): Boolean {
        if (!enabled || !event.isFromSource(InputDevice.SOURCE_MOUSE) || captured.value) return false
        val press = event.actionMasked == MotionEvent.ACTION_DOWN ||
            event.actionMasked == MotionEvent.ACTION_BUTTON_PRESS
        if (press) capture()
        // Hover moves pass through; clicks never reach the touch controls.
        return event.actionMasked != MotionEvent.ACTION_HOVER_MOVE &&
            event.actionMasked != MotionEvent.ACTION_HOVER_ENTER &&
            event.actionMasked != MotionEvent.ACTION_HOVER_EXIT
    }

    private fun capture() {
        val target = view ?: return
        target.requestFocus()
        target.requestPointerCapture()
    }

    /** The game ended: give the pointer back and leave mouse events alone. */
    fun stop() {
        enabled = false
        release()
    }

    /** Gives the pointer back. Returns true if it was captured. */
    fun release(): Boolean {
        val wasCaptured = captured.value
        view?.releasePointerCapture()
        captured.value = false
        return wasCaptured
    }

    /** The window lost or regained pointer capture (focus changes, Esc). */
    fun onCaptureChanged(hasCapture: Boolean) {
        captured.value = hasCapture
        if (!hasCapture) buttons = 0
    }

    private fun onCaptured(event: MotionEvent): Boolean {
        if (!enabled) return false
        captured.value = true
        event.device?.name?.let { if (deviceNames.size < 4) deviceNames += it }
        // Relative movement since the last event, including batched samples.
        var dx = 0f
        var dy = 0f
        for (i in 0 until event.historySize) {
            dx += event.getHistoricalX(i)
            dy += event.getHistoricalY(i)
        }
        dx += event.x
        dy += event.y
        if (dx != 0f || dy != 0f) session.look(dx * LOOK_PER_COUNT, -dy * LOOK_PER_COUNT)
        // Act on newly pressed buttons, whatever action carries them.
        val state = event.buttonState
        val pressed = state and buttons.inv()
        if (pressed and MotionEvent.BUTTON_PRIMARY != 0) session.breakBlock()
        if (pressed and MotionEvent.BUTTON_SECONDARY != 0) session.placeBlock()
        buttons = state
        return true
    }

    private companion object {
        /** Radians per mouse count: about 0.15 degrees, a common PC default. */
        const val LOOK_PER_COUNT = 0.0026f
    }
}
