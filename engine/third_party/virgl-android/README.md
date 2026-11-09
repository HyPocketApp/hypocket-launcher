# VirGL server for Android

The guest's GL calls reach the phone's GPU through VirGL: Mesa's virgl driver
(see `../mesa-guest`) sends commands over a Unix socket to
`virgl_test_server`, which runs them on Android's own OpenGL ES driver.

`build.sh` builds, with the NDK, for Android arm64:

| Project | Version | License |
|---------|---------|---------|
| [libepoxy](https://github.com/anholt/libepoxy) | 1.5.10 | MIT |
| [virglrenderer](https://gitlab.freedesktop.org/virgl/virglrenderer) | 1.3.0 | MIT |

Changes for Android:

- `android-egl.patch`: build virglrenderer's EGL backend without GBM on
  Android, as upstream already does on Windows and macOS, and let
  `vrend_winsys_init` open `EGL_DEFAULT_DISPLAY` itself (upstream's no-GBM
  path only accepts a display handed in by an embedder). The default display
  is the phone's own driver.
- `android-display.patch` and `display/` (our own code, GPL-3.0-or-later):
  direct display. When the app sets `HYPOCKET_DISPLAY_SOCKET`, frames reach
  the app as AHardwareBuffers over a Unix socket and never go through the
  CPU. Our Mesa (`../mesa-guest`) asks the server to adopt each pbuffer image
  it will present (`VCMD_HYPOCKET_ADOPT`, id 65): the server backs that
  resource's texture with a new AHardwareBuffer, so the guest draws straight
  into what the screen shows, and a present (`VCMD_HYPOCKET_PRESENT`, id 64)
  only sends a fence. Anything else presented is copied on the GPU into one
  of three buffers instead. The older trigger, a full-size read of a 2D
  colour resource (optionally skipping the read-back with
  `HYPOCKET_SKIP_READBACK`), is still there for the earlier tests.
- Also in `android-display.patch` (all switched on by the app's environment
  or harmless without it): precise fences with a shared page the server
  hands to our Mesa as a memfd (`VCMD_HYPOCKET_FENCE_WAIT`, 66, and
  `VCMD_HYPOCKET_FENCE_PAGE`, 67), written from the fence thread as soon as
  the GPU finishes a command buffer; shared memory for the app and the game
  (`HYPOCKET_MEMFDS`, the audio ring); no fence fd export unless a client
  asks for one; GL errors checked once per command buffer
  (`HYPOCKET_ERROR_CHECK_EACH` for every command); draws that skip GL calls
  setting what is already set (`HYPOCKET_NO_DRAW_CACHE` turns that off); and
  with `HYPOCKET_PROFILE`, where command and draw time goes, plus
  `hypocket-stats:` and `hypocket-memory:` lines in the log.
- `shim/`: two tiny headers that stand in for AOSP-internal headers
  (`<log/log.h>`, `<cutils/properties.h>`) using the NDK's public APIs.

The server is started by the app as `libvirgl_server.so`, because Android only
lets apps run files named `lib*.so` from their native library directory.
