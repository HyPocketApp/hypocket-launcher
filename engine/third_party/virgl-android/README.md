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
  direct display. When the app sets `HYPOCKET_DISPLAY_SOCKET`, the server
  copies each frame the guest reads back into an AHardwareBuffer on the GPU
  and passes it to the app over a Unix socket, so the app can hand it to the
  system compositor without any CPU copy. With `HYPOCKET_SKIP_READBACK` the
  copy back to the guest is skipped as well. For now a "frame" is detected
  as a full-size read of a 2D colour resource; a real present command from
  Mesa will replace that.
- `shim/`: two tiny headers that stand in for AOSP-internal headers
  (`<log/log.h>`, `<cutils/properties.h>`) using the NDK's public APIs.

The server is started by the app as `libvirgl_server.so`, because Android only
lets apps run files named `lib*.so` from their native library directory.
