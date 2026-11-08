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
 * texture of exactly width x height. Returns true when it was handled.
 */
bool hypocket_display_present(uint32_t res_id, uint32_t width, uint32_t height);

#endif
