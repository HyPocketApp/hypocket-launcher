package io.github.hypocketapp.blocks.ui

import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput

/**
 * For a panel over the game (the menu, the inventory): touches on it never
 * reach the touch controls or the game underneath.
 *
 * It only needs to be hit: of overlapping siblings, only the topmost one
 * under the finger gets the touch. It must not consume anything, because it
 * is the parent of the panel's buttons: a tap whose small finger movements
 * were consumed by a parent is cancelled, so buttons took several tries.
 */
fun Modifier.coverGame(): Modifier = pointerInput(Unit) {
    awaitPointerEventScope { while (true) awaitPointerEvent() }
}
