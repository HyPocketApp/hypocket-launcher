/*
 * HyPocket direct display for the VirGL server (GPL-3.0-or-later, part of
 * HyPocket Launcher). Shows guest frames on the app's screen without copying
 * them back to the guest. See hypocket_display.c.
 */
#ifndef HYPOCKET_DISPLAY_H
#define HYPOCKET_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

/* True when the app asked for direct display (HYPOCKET_DISPLAY_SOCKET is set). */
bool hypocket_display_enabled(void);

/* True when the guest's read-back can be skipped because the frame is shown directly. */
bool hypocket_display_skip_readback(void);

/*
 * Shows the renderer resource res_id on the app's screen if it is a 2D colour
 * texture of exactly width x height. y0_top is true for window-system images
 * (an EGL pbuffer), whose first row is the top one, and false for textures
 * the guest rendered as GL framebuffer objects (first row at the bottom).
 * Returns true when it was handled.
 */
bool hypocket_display_present(uint32_t res_id, uint32_t width, uint32_t height, bool y0_top);

/*
 * Backs a new pbuffer image (a resource nothing has drawn into yet) with a
 * buffer the app's screen shows directly, and sends it to the app, so
 * presenting it later needs no copy. Does nothing when that is not possible.
 */
void hypocket_display_adopt(uint32_t res_id);

/* The resource res_id is gone: frees the screen buffer behind it, if any. */
void hypocket_display_forget(uint32_t res_id);

#endif
