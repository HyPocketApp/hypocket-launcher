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
  Android, as upstream already does on Windows and macOS. It then uses
  `eglGetDisplay(EGL_DEFAULT_DISPLAY)`, which is the phone's driver.
- `shim/`: two tiny headers that stand in for AOSP-internal headers
  (`<log/log.h>`, `<cutils/properties.h>`) using the NDK's public APIs.

The server is started by the app as `libvirgl_server.so`, because Android only
lets apps run files named `lib*.so` from their native library directory.
