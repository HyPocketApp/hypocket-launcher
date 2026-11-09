# Mesa for the guest (arm64 glibc)

Box64 forwards the guest's EGL/GLES calls to native arm64 libraries. These
are Mesa built with only the virgl Gallium driver, which sends the work to the
VirGL server on Android (see `../virgl-android`).

`build.sh` downloads the official Mesa 26.2.6 release, checks its SHA-256 and
builds `libEGL.so.1`, `libGLESv2.so.2` and `libgallium-26.2.6.so`. It also
copies `libdrm`, `libexpat` and `zlib` from the build machine's arm64
packages.

The app runs the guest with `GALLIUM_DRIVER=virpipe`,
`LIBGL_ALWAYS_SOFTWARE=1`, `EGL_PLATFORM=surfaceless` and
`VTEST_SOCKET_NAME` pointing at the server's socket.

Licenses: Mesa, libdrm and expat are MIT; zlib is under the zlib license.

## Changes

- `hypocket-present.patch` (GPL-3.0-or-later, part of HyPocket): when
  `HYPOCKET_PRESENT` is set, `eglSwapBuffers` on a surfaceless pbuffer sends
  a `VCMD_HYPOCKET_PRESENT` command (id 64) with the pbuffer's resource to
  the VirGL server, which shows it on the phone's screen without copying it
  back (see `../virgl-android/display`). After the first present the
  pbuffer draws into three images in turn, and each one is announced to the
  server before it is used (`VCMD_HYPOCKET_ADOPT`, id 65), so the server can
  back it with a buffer the screen shows directly and no copy is needed.
  Without the variable nothing changes.
- Same patch: with `HYPOCKET_FENCES=1`, precise fences. A fence is the number
  of a command buffer, and the guest checks and waits for it in a page the
  server shares (it asks for the page's fd with `VCMD_HYPOCKET_FENCE_PAGE`,
  67), sleeping on a futex the server wakes, with no round trip.
