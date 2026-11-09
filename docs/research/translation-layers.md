# Research: translation layers

Status: research notes, November 2026. Nothing here is integrated yet.

This document collects what we know about running the PC game client on
Android ARM64, which options exist for each layer, and the plan for the
`:engine` module. It is a living document: update it when something is
verified or proven wrong.

## 1. What we need to run

Sources: the official system requirements page and the bring-up notes of
the Vexa project (see section 5). Items marked *(Vexa notes)* come from that
project's investigation and have not been verified by us yet.

- **Client build:** the Linux x86-64 build is the best target. It is a native
  ELF binary (`HytaleClient`) that links `libc`, `libm` and the dynamic loader,
  so it can run on Linux-style compatibility layers without Wine.
  *(Vexa notes)*
- **Libraries loaded at runtime** *(Vexa notes)*: glibc and libstdc++, OpenSSL,
  DNS/NSS libraries, OpenAL Soft (audio), SDL, D-Bus, X11/XCB (needed by the
  UI library, can be stubbed) and `libNoesis.so` (the UI library shipped with
  the game).
- **Network:** the client fetches signing keys from the official session
  service over HTTPS, so the guest needs working DNS and CA certificates.
  *(Vexa notes)*
- **Graphics:** OpenGL 4.1 or newer. The official page says later versions may
  require Vulkan 1.3.
- **Official minimum PC:** Core i5-7500 or Ryzen 3 1200 class CPU, 8 GB RAM,
  1080p at 30 FPS.

### Honest expectation for low-end phones

The official minimum is a desktop CPU with 8 GB of RAM, and x86-64
translation costs extra CPU and memory. Running the game on a phone with
3 to 4 GB of RAM is a long-term goal, not a first milestone. The realistic
plan is:

1. First make it start at all on a mid-range or high-end phone.
2. Then work down: lower render resolution, frame-rate limits, view distance,
   memory trimming and translation caches.

## 2. CPU: x86-64 to ARM64

| Option | License | Notes |
|--------|---------|-------|
| [Box64](https://github.com/ptitSeb/box64) | MIT | Dynamic recompiler. **Wrapped libraries**: calls to common libraries (GL, SDL, OpenAL, libc, ...) can go to native ARM64 versions instead of being emulated, which saves a lot of CPU. Widely used on Android (inside glibc environments such as those of Winlator-style apps). Native Android/Termux mode is marked experimental upstream. |
| [FEX](https://github.com/FEX-Emu/FEX) | MIT | Dynamic recompiler with high accuracy. Uses **thunks** to forward library calls (e.g. GL) to the host. Designed for Linux; Vexa found it needs an Android integration layer and NDK 29 (`std::atomic_ref`). |

**Recommendation:** prototype both behind the same small `:engine` API and
measure on the same device before choosing. Box64 looks like the better
first try for weak phones because of wrapped libraries; FEX looks more
accurate and is what Vexa already proved can start x86-64 programs on
Android.

Both should be **pinned upstream dependencies** (git submodules at a fixed
commit, built with the NDK), not copied into our tree. Changes we need should
be sent upstream when possible.

## 3. Graphics: desktop OpenGL 4.1 to the phone GPU

| Option | License | What it does | Fit |
|--------|---------|--------------|-----|
| [MobileGlues](https://github.com/MobileGL-Dev/MobileGlues) | LGPL-2.1 | Desktop GL on top of OpenGL ES 3.x (best on 3.2). Built for Minecraft: Java Edition launchers. | Best first candidate: modern GL profile, works on most GPUs with GLES 3.2. Must be loaded as a separate shared library (LGPL). |
| Zink (part of [Mesa](https://gitlab.freedesktop.org/mesa/mesa)) | MIT | Desktop GL on top of Vulkan. | Good on Adreno with the Turnip driver (also Mesa). Weak or broken Vulkan drivers on many cheap phones. Second path. |
| [gl4es](https://github.com/ptitSeb/gl4es) | MIT | OpenGL 2.1 on top of GLES 2/3. | Too old for a GL 4.1 core client. Useful only as a reference. |
| [ANGLE](https://github.com/google/angle) | BSD-3-Clause | GLES on top of Vulkan (and others). | Does not provide desktop GL. Possible lower layer under MobileGlues on devices with bad GLES drivers. |

**Recommendation:** start with MobileGlues on GLES 3.2, forwarded from the
guest through the CPU translator's wrapped library or thunk. Keep Zink +
Turnip as an option for Adreno devices.

Phone requirement that follows from this: **OpenGL ES 3.2** (or Vulkan 1.1+
for the Zink path). We should check this in the app before allowing a launch.

## 4. Everything else

- **Window and input:** the client uses SDL. We need its window and input to
  reach an Android `SurfaceView` (SDL has an Android backend; the question is
  how the guest SDL connects to it). Termux:X11 (GPL-3.0) is another option
  Vexa uses, but it adds an X server.
- **Audio:** OpenAL Soft (LGPL-2.0-or-later) has Android backends (AAudio,
  OpenSL ES). Route the guest's OpenAL calls to a native ARM64 build.
- **Guest root filesystem:** the guest needs x86-64 glibc and related
  libraries. These are open-source distribution packages (mostly LGPL/GPL),
  not game files. Prefer downloading them from an official distribution
  mirror at first launch, with checksums, instead of putting them in the APK.
  If we ship any, we must also provide their source.
- **Game files:** downloaded only with the player's account (see
  `:core`). `libNoesis.so` and every other file that comes with the game are
  game files: never commit, bundle or mirror them.

## 5. Notes on Vexa

[Vexa](https://github.com/CriticalRange/Vexa) is a general Android runtime for
Linux x86-64 programs, first aimed at this game. Takeaways:

- Architecture: a Kotlin "operator console" app and a separate native
  runtime, with an explicit launch contract between them. This matches our
  `:app`/`:core`/`:engine` split.
- It uses upstream FEX through a thin Android layer, a prepared rootfs in
  app-private storage, GL thunks to the host driver and in-app logs.
- Practical lessons: validate the rootfs and loader before launch, run guest
  files from internal storage (not shared storage), run the translator on a
  dedicated native thread with an explicit stack size, log every startup
  phase, and record the device model in crash reports (MIUI/HyperOS caused
  extra instability).

**License: the Vexa repository has no license file** (checked November
2026). Without a license, all rights are reserved, so **we must not copy any
of its code**, scripts or files. We can learn from its public notes and write
our own implementation. If we ever want to reuse something, we must ask the
author to license it under terms compatible with GPL-3.0 first.

## 6. License summary

| Component | License | Compatible with GPL-3.0 |
|-----------|---------|-------------------------|
| Box64 | MIT | Yes |
| FEX | MIT | Yes |
| Mesa (Zink, Turnip) | MIT (mostly) | Yes |
| gl4es | MIT | Yes |
| MobileGlues | LGPL-2.1 | Yes (keep it as a separate shared library) |
| ANGLE | BSD-3-Clause | Yes |
| SDL | zlib | Yes |
| OpenAL Soft | LGPL-2.0-or-later | Yes (separate shared library) |
| Termux:X11 | GPL-3.0 | Yes |
| Vexa | None (all rights reserved) | **No: study only** |

Licenses were read from each project's license file in November 2026.
Check again before integrating, and keep the copyright notices.

## 7. Proposed plan for `:engine`

Each step should end with something we can test on a real phone.

Progress:

- Step 1 started (November 2026): Box64 v0.4.4 is a submodule in
  `engine/third_party/box64`, built with NDK 28 for Android API 28 (lower
  API levels lack some pthread functions Box64 uses). It is packaged as
  `libbox64.so` and runs `engine/guest-tests/probe` from **Settings >
  Translator test**. Verified so far: the probe gives the correct result
  under Box64 on an x86-64 Linux host (interpreter only).
- First phone test (Samsung Galaxy M12, Exynos 850, Mali-G52, 4 GB RAM,
  Android 13): Box64 starts, enables the ARM64 dynarec and loads the probe,
  then fails with `Global Symbol __libc_start_main not found`.
  **Cause:** with `-DANDROID`, Box64 targets x86-64 *Android* (bionic)
  programs. Its wrapped libc only provides bionic's `__libc_init` entry
  point (`src/emu/entrypoint.c`), not glibc's `__libc_start_main`, and
  upstream says the native Android build "won't run linux binaries". The
  game is a glibc Linux program, so this setup cannot run it as is.

- Second setup (chosen): Box64 built for **Linux glibc** on arm64, shipped
  with a minimal ARM64 glibc runtime taken from the cross compiler
  (`ld-linux-aarch64.so.1`, `libc`, `libm`, `libdl`, `libpthread`, `librt`,
  `libutil`, `libresolv`, `libgcc_s`; about 3 MB). The app starts glibc's
  loader, which loads Box64, which runs the probe. Files are renamed to
  `lib*.so` in the APK and linked back to their real names at run time.
  Verified with the APK's own files under qemu-aarch64: correct checksum.
  **Not yet tested on a phone.** The GPU question (section below) is still
  open for this setup: on Mali it will need VirGL or libhybris.
- Licenses of the shipped runtime: glibc is LGPL-2.1-or-later and libgcc_s
  is GPL-3.0 with the GCC Runtime Library Exception. Both are compatible
  with GPL-3.0, but a public release must offer their source code.

- Third phone test (same device): exit code 159, which is 128 + SIGSYS,
  before any output. Android 13 does not allow apps to call
  `set_robust_list`, `rseq`, `clone3`, `faccessat2` or `fchmodat2`
  (checked against bionic's `SYSCALLS.TXT` and seccomp allowlists for
  android13-release), and glibc calls `set_robust_list` at startup. Fixed
  by building glibc 2.39 with `engine/third_party/glibc-android`. Under
  qemu the probe passes and only allowed system calls remain.
- **Fourth phone test (same device): PASSED.** Box64 with the patched glibc
  runs the x86-64 probe with the correct checksum.

  | Measure | Result |
  |---------|--------|
  | Native ARM64 workload | 380 ms |
  | Same workload translated by Box64 | 441 ms (about 1.16x native) |
  | Total, including starting glibc, Box64 and the probe | 2614 ms |
  | Peak memory of the Box64 process | 31.5 MB |

  The ratio is optimistic: `memcpy`, `malloc` and other libc calls run as
  native code through Box64's wrapped libc, so only part of the workload is
  translated. Startup time is mostly Box64 initialising (it also tries to
  run `lscpu` through `/bin/sh`, which does not exist on Android). Since
  November 2026 the app passes the CPU name, core count and frequency in
  `BOX64_SYSINFO_*` variables, so Box64 no longer runs `lscpu`. On the
  phone this cut the total from 2614 ms to 608 ms (native 309 ms,
  translated 382 ms): starting glibc, Box64 and the probe now takes about
  0.2 s instead of 2.2 s.

### Graphics proof of concept (November 2026)

Path: x86-64 GL program → Box64 (wrapped `libEGL`/`libGLESv2`) → Mesa 26.2.6
virgl driver built for arm64 glibc → Unix socket → virglrenderer 1.3.0
`virgl_test_server` built for Android → the phone's OpenGL ES driver.

- `engine/guest-tests/gl-probe` draws a triangle off screen (EGL pbuffer),
  reads two pixels back and checks their colors.
- Verified with the APK's own files under qemu-aarch64, talking to an x86-64
  build of the same server using Mesa's software renderer: correct colors.
- The Android build of the server needed one build-system patch (EGL
  without GBM) and two small header shims.
- First phone test: the server failed with "EGL is not supported on this
  platform" (virglrenderer's no-GBM path only accepted an external display).
  Fixed by opening `EGL_DEFAULT_DISPLAY` on Android.
- **Second phone test (Galaxy M12, Mali-G52, Android 13): PASSED.** The
  x86-64 probe, translated by Box64, drew the triangle on the phone's GPU
  with correct colors. `GL_RENDERER` is `virgl (Mali-G52)`, `GL_VERSION`
  is `OpenGL ES 3.1 Mesa 26.2.6`. 120 trivial frames took 52 ms and the whole
  run about 0.8 s.
- The game needs **desktop OpenGL 4.1 core**, not GLES. Third phone test,
  desktop GL probe (same device):

  | Check | Result |
  |-------|--------|
  | Server sees | OpenGL ES 3.2 driver, GLSL feature level 430 |
  | Desktop GL offered by virgl (no override) | 2.1 compatibility only; no core context at all |
  | Missing for 3.0 | `GL_NV_conditional_render` |
  | Missing for 3.2 | `GL_ARB_depth_clamp` |
  | Missing for 3.3 | `GL_ARB_blend_func_extended` |
  | Missing for 4.0 | `GL_ARB_gpu_shader_fp64`, `GL_ARB_texture_query_lod` |
  | Missing for 4.1 | `GL_ARB_vertex_attrib_64bit`, `GL_ARB_viewport_array` |
  | Max texture size | 16383 (4.1 needs 16384) |
  | **With `MESA_GL_VERSION_OVERRIDE=4.1`** | **4.1 core context created; triangle drawn with correct colors** |

  So the Mali-G52 lacks a handful of features for a "real" 4.1, but a forced
  4.1 core context works for basic drawing. Whether the game itself uses any
  of the missing features (depth clamp, dual-source blending, viewport arrays,
  doubles in shaders, `textureQueryLod`) is unknown until the client runs;
  some could be emulated in virglrenderer or Mesa if needed.

### Display proof of concept (November 2026)

`engine/guest-tests/gl-display-probe` draws an animated scene (576 spinning
quads, one instanced draw, desktop OpenGL 4.1 core with the version override)
at 640x360, reads every frame back with `glReadPixels` into a shared file,
and the app shows it on a `SurfaceView` at each screen refresh (**Settings >
Display test**).

This read-back path is temporary: it is how VirGL with X11 works in Termux,
but it costs a GPU-to-CPU copy per frame. The final path should present the
guest's frames without the copy, most likely through a custom video backend
in a native (wrapped) SDL build and a present command in the VirGL server.

Verified under qemu with the GLES-only host server (correct image).

**Phone test (Galaxy M12, Mali-G52): PASSED.** The scene showed on screen,
smooth at about 60 fps (the display's refresh rate) for the whole run; the app
showed 887 frames in 16.3 s including start-up. The guest itself produced
153 fps: 0.1 ms to submit the draw and 6.5 ms for `glReadPixels`, which also
waits for the GPU to finish, so the 6.5 ms is GPU time plus the copy. At
640x360 the read-back path is not the bottleneck yet; at the game's
resolution it will be (the copy grows with the pixel count).

### Direct display, stage 1 (November 2026)

To remove the per-frame copy, the VirGL server now can show frames itself
(`engine/third_party/virgl-android/display`, `engine/src/main/cpp/direct_display.cpp`):

1. The app listens on an abstract Unix socket; only processes of the app's
   own Linux user may connect.
2. The server allocates three `AHardwareBuffer`s of the frame size, sends
   them to the app, and for every frame copies the guest's texture into a
   free one on the GPU with a small shader, with a native fence.
3. The app imports the buffers as EGL images and draws each frame onto the
   `SurfaceView`'s own window with OpenGL ES (one GPU pass, no CPU copy),
   then tells the server when the previous buffer is free. If none is free,
   the server drops the frame.

The first try handed the buffers to the compositor through a child
`ASurfaceControl` of the `SurfaceView`, which would avoid even the GPU pass,
but on the Galaxy M12 nothing showed (the frames reached the compositor and
the screen stayed black, even with a grey test fill), so the app draws them
itself for now.

For now the server treats a full-size read-back of a 2D colour resource as
"the frame", and skips the copy back to the guest. Stage 2 replaces that
with a present command sent by Mesa on `eglSwapBuffers`, so programs no
longer need `glReadPixels` at all. **Phone test (Galaxy M12): WORKS** after three fixes found on the phone:
the copy is a shader (a blit between BGRA and RGBA targets fails silently),
it runs after a one-pixel read through the renderer (virglrenderer switches
to the client's GL context lazily), and the display context is created in
the guest contexts' share group (virglrenderer's first context does not
share with the EGL setup context, so texture names pointed elsewhere). The
centre pixel of the first frame matched on both sides (39 70 31) and the
terrain showed on screen, 758 frames in 27.7 s.

Frame times are not better than the copying path yet (light 12–30 ms,
medium 21–24 ms, heavy 57–64 ms), because the one-pixel read makes the
server wait for the GPU on every frame and the program still calls
`glReadPixels`. Stage 2 removes both.

### Direct display, stage 2 (November 2026)

The present now comes from the program itself, with no read-back at all:

- `engine/third_party/mesa-guest/hypocket-present.patch`: with
  `HYPOCKET_PRESENT` set, Mesa lets `eglSwapBuffers` on a surfaceless pbuffer
  reach the driver (EGL normally ignores it), flushes and sends a new vtest
  command, `VCMD_HYPOCKET_PRESENT` (64), with the pbuffer's resource.
- The VirGL server (`android-display.patch`) switches to the client's GL
  context without waiting for the GPU and shows the resource through the
  direct display. Window-system images have their first row at the top, so
  the copy does not flip them.
- `gl-perf-probe` shows each frame by blitting its framebuffer object to a
  pbuffer of the frame's size and calling `eglSwapBuffers`.

This is the path a game's window layer would use: for example SDL3's
offscreen video driver renders to an EGL pbuffer and swaps it. Verified
under qemu (the server receives a present per swap).

**Phone test (Galaxy M12): WORKS.** After accepting 10-bit colour (EGL
picks `B10G10R10X2` for a pbuffer asked for "8 bits or more"), the terrain
showed upright with the right colours, 998 frames in 28.5 s, and the
program's per-frame "copy" (now only the swap) dropped to 0.2 ms.

| Render scale | Light | Medium | Heavy |
|--------------|-------|--------|-------|
| 50%  | 16.2 ms | 27.1 ms | 66.5 ms |
| 75%  | 15.8 ms | 27.7 ms | 63.3 ms |
| 100% | 16.4 ms | 27.6 ms | 64.3 ms |

The draw times are a few milliseconds higher than with the read-back path
(about 11, 21 and 57 ms), and the light load sits at about 16 ms whatever
the resolution. The GPU now also does the server's copy and the app's draw
every frame, and the test's `glFinish` waits behind them; the light case
looks tied to the 60 Hz display. A game does not call `glFinish` every
frame, so its work can overlap with presenting, but this deserves a closer
look (for example rendering straight into the shared buffers, which would
remove the server's copy).

### Draw calls and geometry: the fences were the bottleneck (November 2026)

The new benchmark (heavy world, fixed camera path, no FPS limit) on two
phones, before any change:

| Phone | fps | game CPU ms | wait ms | server ms/frame | half resolution |
|---|---|---|---|---|---|
| Galaxy M12 (Mali-G52) | 28.3 | 19.1 | 16.3 | 15.1 (39% busy) | 33.3 fps |
| Xiaomi SM7435 (Adreno 710) | 93.4 | 4.8 | 6.0 | 3.1 (27% busy) | 95.0 fps |

Mesa's glthread made the Adreno phone slower (73 fps) and changed nothing
on the M12; Box64's SAFEFLAGS=0 and SEP=1 changed nothing either. Pixels
mattered little. On the M12, game CPU plus server time added up to the
frame time, so the two were taking turns rather than working in parallel.

The cause is in the vtest protocol's fences: each fence creates a
resource, a round trip in protocol 3 that waits until the server has
worked through everything queued; VCMD_RESOURCE_BUSY_WAIT answers whether
the GPU finished everything sent so far, whatever resource it is asked
about, so waiting for the previous frame also waits for the current one
and any "is this buffer busy" check means "is the GPU idle"; and timed
waits poll every 10 microseconds. Our Mesa and server now use precise
fences (command buffer numbers, per-resource busy tracking, and a shared
page with the last finished number plus a futex, so no round trips at
all). Built for a PC and run there with llvmpipe, the heavy benchmark went
from 55.9 to 75.0 fps, the game's CPU time from 13.8 to 0.6 ms per frame,
and the server from 70% to 92% busy. The server's command processing is
now the part to speed up next.

On the phones, with the precise fences (heavy benchmark):

| Phone | old fences | HyPocket fences | game CPU ms (old → new) |
|---|---|---|---|
| Galaxy M12 (Mali-G52) | 28.1 fps | 59.9 fps | 19.3 → 4.3 |
| Galaxy A24 (Mali-G57) | 59.9 fps | 60.1 fps | 13.3 → 3.9 |
| Xiaomi SM7435 (Adreno 710) | 105.6 fps | 119.9 fps | 4.1 → 1.8 |

Fence checks went from about 1000-1400 per second to none. Every phone
then sat at its screen's refresh rate (60 or 120 Hz), so the benchmark now
runs without that limit and profiles the server's commands. On the M12
the server spends about 13 ms per frame on the heavy scene's commands,
which caps it near 75 fps.

An LDPlayer emulator (Android 9) could not run the server: it found no
OpenGL ES driver. Emulators are not supported for now.

### Late fences, and where a draw call's time goes (November 2026)

Without the refresh-rate limit the M12 still stopped near 60 fps, with the
server 76% busy: game CPU (4.2 ms) plus server time (13.2 ms) again added up
to the frame time. A trace on a PC showed why: the server's single thread
only retires fences between command buffers, so the game heard that frame N
was done only after the server had also worked through all of frame N+1,
and the server then idled while the game prepared the next frame. The
server's fence thread now writes the shared fence page as soon as the GPU
finishes a command buffer. Benchmark with the server's options turned off
one at a time (heavy scene, each box its own draw call, about 410 draw calls
per frame):

| Phone | before | GL error check after every command | fences on the main thread | both on |
|---|---|---|---|---|
| Galaxy M12 (Mali-G52) | 57.6 fps | 66.5 fps | 59.1 fps | 67.9 fps |
| Xiaomi SM7435 (Adreno 710) | 89.8 fps* | 129.7 fps | 138.9 fps | 134.0 fps |

\* First phase on a cold phone; the benchmark now warms up first.

On the M12 the server is now 87-91% busy. The Xiaomi is limited by pixels
(236 fps at half resolution), so neither change shows there. Inside the
server's draw command (DRAW_VBO, about 24 us on the M12):

| Step | M12 (us per draw) |
|---|---|
| The driver's own glDraw call | 18.7 |
| Uniform buffers, constants and samplers | 1.6 |
| Shader program selection | 1.3 |
| Vertex buffers | 1.2 |
| Everything else | 1.2 |

Three quarters of the time is inside the Mali driver, so fewer draw calls
is what helps most: HyPocket Blocks now draws its creatures and particles
with one instanced draw call (about 160 draw calls per frame instead of
410). The benchmark keeps separate draws in its phases, the same load in
every version, and adds one phase with instancing.

### v39 results, and a leaner server (November 2026)

The heavy benchmark with v39 (fps; "instanced creatures" draws the
creatures with one instanced draw, the other phases with one draw each):

| Phone | before | error check each | late fences | default | half resolution | instanced creatures |
|---|---|---|---|---|---|---|
| Galaxy M12 (Mali-G52) | 59.3 | 68.9 | 57.5 | 68.7 | 72.3 | 117.8 |
| Galaxy A24 (Mali-G57) | 165.9 | - | - | 175.4 | 193.5 | 195.4 |

- On the M12 a DRAW_VBO costs about 24 us, 19 of them inside the Mali
  driver's glDraw call. Half the resolution gains only 5%: the limit is the
  number of draw calls, not pixels. Instancing the creatures (fewer draws)
  gains 71%.
- On the A24 a DRAW_VBO costs about 3.6 us and the server is only 54% busy;
  the rest looks like waiting for the GPU.

What changed after v39, in the server (`virgl-android/android-display.patch`)
and our Mesa (`mesa-guest/hypocket-present.patch`):

- **"failed to export fence sync object" (`virgl_fence_set_fd: failed
  err=-9`)**: seen 3 times in 99 s of play on the M12, harmlessly. Upstream
  exports every fence of the renderer's own context as a sync file (a dup
  of the driver's native fence fd, kept in a table) for clients that ask
  for fence fds. Our Mesa never asks, so this was only cost: an
  `eglDupNativeFenceFDANDROID`, an fd dup and a poll of every stored fence
  on each command buffer. The error is EBADF from that dup, so the driver
  most likely handed back an fd number that was not open; the sync thread
  waits on (and dups) the same fence at the same time, which fits a rare
  race (not confirmed on the phone). Closing such a number blindly
  could close some other fd that took it meanwhile, so the export is now
  off unless a client asks for fence fds (DRM syncs or SUBMIT_CMD2 out
  fences), which takes the race away rather than hiding the message.
- **Shared pages no longer live in files.** The fence page, which the
  server's sync thread writes after every command buffer, was a file in the
  app's cache, and so was the audio ring. A shared mapping of a file is
  written back to storage now and then (dirty pages expire after about 30
  s), and a write to a page meanwhile waits in the file system, which on the
  M12's eMMC can take tens of milliseconds. That matches the 4 audio
  underruns in 96 s (the game keeps 60 ms of sound ahead) and none on the
  A24, which has faster UFS storage; it is a hypothesis until the next
  report. The fence page is now a memfd the server hands to our Mesa over
  the vtest socket (`VCMD_HYPOCKET_FENCE_PAGE`, 67), so it can never touch
  storage. The audio ring is opened by the game by path, and Android gives
  neither the app nor the game a way to pass the other an fd, so the server
  makes it as a memfd too (`HYPOCKET_MEMFDS`, Android 11 and later) and both
  sides open the server's `/proc/<pid>/fd/<n>`. If the app cannot open it
  (SELinux may refuse), the ring stays a file; the audio line of the report
  says "ring in memory" or "ring in a file".
- **Memory.** The server now logs where its resident memory is, from
  `/proc/self/smaps`, after 20 s and at the end (`hypocket-memory:` lines):
  the GPU driver's mappings, the guest's resources (memfds the game maps as
  well, so the "VirGL server" peak and the game's peak count them twice),
  malloc, other anonymous memory and code. The A24 uses about 180 MB
  against 115 MB on the M12; part of it is its larger screen (2340x1080
  against 1600x720: three presented images, the depth buffer and the 3D
  framebuffer are 2.2 times bigger, about 30 MB more), the rest will show in
  that breakdown. Freed malloc memory now goes back to the system after
  about a second (`M_DECAY_TIME`).
- **Fewer GL calls per draw.** In DRAW_VBO, the steps the profile calls
  "select", "objects" and "vertices":
  - select: upstream picks the shader program again whenever any vertex
    buffer changed, which is nearly every draw of a game (each chunk has its
    own buffer) and builds and compares the shader keys of the vertex
    shader three times and the fragment shader once, to find the same
    program. The vertex shader's key only depends on the vertex elements'
    integer and BGRA attributes, so the program is now picked again only
    when those change (or for upstream's other reasons).
  - objects: the uniform block with the system values (y flip, alpha
    reference) was bound again with `glBindBufferRange` on every draw, and
    the other uniform buffers whenever they were marked dirty, even with the
    same buffer and range. A binding change can make the driver check its
    state again in the next glDraw, which is where most of the time goes on
    the Mali-G52, so ranges already bound are skipped. Constants identical
    to the last ones are not uploaded again.
  - vertices: the vertex array object is bound only when it is not the one
    already bound.
  - The GLES-only lookup of the texture level uniforms (a string format
    and a driver lookup per stage) runs once per program instead of on
    every draw.

  These caches are per GL context and are dropped whenever other code binds
  a vertex array object or deletes a buffer or vertex array object, so a
  reused name is never taken for the old one. `HYPOCKET_NO_DRAW_CACHE`
  turns them off, to compare or to rule them out if something draws wrong.

  On a PC (the server on llvmpipe, heavy scene, separate draws, 800x360,
  two runs each way) the steps dropped as expected: select from 18.6-19.9
  to 7.9-10.5 ms per 600 command buffers, objects from 35.6-35.9 to
  29.9-34.9 and vertices from 18.1-18.2 to 15.7-17.7. The frame rate did not
  change (65.5-67.3 fps either way) because llvmpipe's own draw takes 97% of
  the time; what this is worth on the Mali-G52, where the driver's glDraw is
  much cheaper relative to these steps, needs the phone. The image read back
  after 120 frames matched with and without the caches except for the
  moving creatures (0.15% of the pixels), and the fence page went through
  the socket (0-1 fence round trips per 5 s).
- **Build fix.** `mesa-guest/build.sh` extracts Mesa inside `engine/build`,
  which is inside this repository; `git apply` then found the repository
  and silently skipped every path of our patch, so a build could ship Mesa
  without HyPocket's changes (it happened in a fresh checkout while making
  this change). The script now stops git's search at its work dir and fails
  if the patch did not land.

### A third phone: Helio G99 with Mali-G57 MC2 (November 2026)

A tester's Galaxy A24 (MT6789, Mali-G57 MC2, 3.7 GB of memory, Android 16):

- Translator test: 122 ms natively, 139 ms through Box64 (14% slower).
- HyPocket Blocks in heavy mode at the full 2340x1080, 60 fps limit, for
  277 s: 57-59 fps at 100% for most of the session, 13-16 ms per frame,
  165-230 thousand triangles. Near the end it fell to 55 fps; dynamic
  resolution went down to 50% and the frame rate stayed at 55-57, so pixels
  were not the limit there, yet the scale stayed down. It now raises the
  scale to test that after 10 seconds stuck below the target.
- A USB keyboard and mouse worked (pointer capture, 9337 mouse events), and
  the game counted no audio underruns. The VirGL server peaked at 188 MB on
  this Mali driver, against 82-104 MB on the Galaxy M12.

### A second phone: Snapdragon 7s Gen 2 with Adreno 710 (November 2026)

A tester's Xiaomi phone (SM7435, Adreno 710, 7.3 GB of memory, Android 16):

- Translator test: 122 ms natively, 150 ms through Box64 (23% slower), 309
  ms in total with start-up. Box64 uses the hardware counter here.
- HyPocket Blocks in heavy mode at the screen's full 2712x1220 with a 60 fps
  limit: about 59 fps for most of 147 s, 8-16 ms per frame, 150-220
  thousand triangles and up to about 140 creatures on screen; the world
  built in 378 ms. Dynamic resolution dipped to 55% during two short slow
  stretches and climbed back to 100%. All frames but the first went
  through the direct display without a copy, and the game counted no audio
  underruns.
- Adreno's driver works with the same server unchanged; it logs that
  robustness extensions are missing and a stale GL error during start-up,
  without consequences so far.

### HyPocket Blocks with sound, controllers and heavy mode (November 2026)

Galaxy M12, 1600x720, 60 fps limit, dynamic resolution on:

- Normal mode: 59.5 fps, 11-16 ms per frame. The first dynamic-resolution
  rule lowered the 3D scale to 50% although the game held its frame rate
  (frames came close to the budget while waiting for the GPU); it now acts
  only when frames are actually missed.
- Heavy mode (16 x 16 chunks, view distance 128, 300 creatures with their
  own draw calls, 195-228 thousand triangles, about 120-145 creatures and
  some 250 extra draw calls on screen): 30-35 fps, 28-34 ms per frame, the
  world built in 1.1 s. Halving the 3D resolution barely helped, so this
  load is limited by geometry and draw calls (the guest's CPU, VirGL's
  command path or the GPU's vertex work), not by pixels. Peak memory:
  launcher 132 MB, VirGL server 104 MB, game 44 MB.
- Audio through AAudio: 48 kHz low latency, 240-frame bursts, 480 frames of
  buffer. The game's own log counted 8 and 30 underruns in 77 and 114 s;
  the app's count was inflated by the silence after the game exited, which
  no longer counts, and the game now keeps 60 ms ahead instead of 40. With
  that change a 57 s session had 0 underruns and no audible crackles.
- Controllers: a Bluetooth controller (reported as "Wireless Controller")
  works directly once the phone's own controller-to-touch mapping is off;
  speed and buttons felt right.

### Direct display, stage 3: no copy at all (November 2026)

Stage 2 still copied every frame twice on the GPU: the guest blitted its
framebuffer into the pbuffer, and the server copied the pbuffer into an
AHardwareBuffer. Now our Mesa gives a presented pbuffer three images used in
turn and announces each one to the server right after creating it
(`VCMD_HYPOCKET_ADOPT`). The server backs that resource's texture with a new
AHardwareBuffer, so the guest draws straight into what the app shows, and a
present only sends a native fence. HyPocket Blocks also draws into the
pbuffer itself, like a game drawing into its window.

On the Galaxy M12 at 1600x720 with a 60 fps limit: 7199 of 7200 frames were
shown without any copy (the first one is copied on purpose), 59.3 fps, 6-16
ms per frame depending on the view, no visible difference. The time per
frame barely moved because it measures the game's CPU work and the wait for
the previous frame; what the copies cost was GPU time, which at a 60 fps cap
shows up as headroom rather than as a higher frame rate. The VirGL server
grew from 65 to 71 MB because it kept the three copy buffers of the first
frame; it now frees them once frames need no copy.

### Graphics performance test (November 2026)

On phones the game should run in a window with the screen's aspect ratio
and a lower internal resolution scaled up to the full screen (the app's
"Render resolution" setting), not at a desktop resolution, so its UI stays
readable. `engine/guest-tests/gl-perf-probe` therefore measures at 50%, 75%
and 100% of the phone's own screen, with three loads of a textured, lit,
depth-tested block terrain (2048, 8192 and 27648 cubes, every face drawn).
It times draw plus `glFinish` (GPU time through VirGL) separately from the
read-back used for display (**Settings > Graphics performance test**).
Verified through VirGL under qemu.

**Phone test (Galaxy M12, Mali-G52, 1600x720 screen): PASSED** (no GL
errors). Draw plus `glFinish` per frame, with the read-back copy apart:

| Render scale         | Light (24.6k triangles) | Medium (98.3k) | Heavy (332k) |
|----------------------|-------------------------|----------------|--------------|
| 50% (800x360)        | 16.8 ms (59 fps)*       | 20.6 ms (49)   | 39.6 ms (25) |
| 75% (1200x540)       | 11.0 ms (91 fps)        | 22.8 ms (44)   | 41.0 ms (24) |
| 100% (1600x720)      | 13.6 ms (74 fps)        | 26.7 ms (37)   | 59.3 ms (17) |

\* The first step also pays for warming up (shader compile, first use of the
buffers), so it reads slower than the 75% one.

Read-back copy: 3–9 ms per frame, with spikes of 136 ms (first frame at
1600x720, when the bigger frame is first touched) and 33 ms.

What it means:

- **The cost follows the triangle count much more than the pixel count.**
  Going from 50% to 75% of the screen barely changes the time; going from
  98k to 332k triangles doubles it. The scene is a single instanced draw, so
  this is the Mali-G52's vertex throughput (about 8 million triangles per
  second through VirGL), not per-call overhead.
- So lowering the render resolution helps little once the scene is heavy;
  **lowering the view distance** (fewer chunks, fewer triangles) is the
  setting that matters on this class of GPU. The launcher should expose it
  and default it low.
- The test draws every cube face. A real voxel renderer hides the faces
  between blocks, so a world with the "heavy" number of blocks draws far
  fewer triangles; how many the game's client actually draws is unknown
  until it runs.
- 75% looks like the right default for the render scale: 100% costs a bit
  more on the GPU and a lot more on the copy.
- Three more runs gave the same numbers within a few percent (light 10–14
  ms, medium 20–29 ms, heavy 39–62 ms), so the results are repeatable. The
  slow first step of the first run was warm-up. Copy spikes (up to 30 ms at
  1600x720) still appear now and then.
- The tester saw the app crash on some early runs. The report of a crashed
  run is lost, so the app now records why its previous runs ended
  (**Settings > Last app exit**). Three runs in a row with that build did not
  crash, and the exit records only showed normal closes; the phone had
  863 MB free of 3.7 GB with the app idle. The cause is still unknown, so the
  records stay in the app for future reports.

### HyPocket Blocks on the test phone (November 2026)

`blocks/game` (then `engine/guest-tests/blocks`) is a small, original block game written for this
project (chunked meshes with only visible faces, per-vertex ambient
occlusion, water, walking, breaking and placing blocks). It runs through the
whole path: Box64, desktop OpenGL 4.1, VirGL and direct display (stage 2),
with touch input through a shared file. Results on the Galaxy M12 (Exynos
850, Mali-G52), power saving on, 1200x540 (75% of the screen) unless noted:

| FPS limit | Measured fps | Time per frame | Triangles | Session |
|---|---|---|---|---|
| 30 | 29.8 | 9-13 ms | 60-80 thousand | 89 s, 2633 frames |
| 60 | 59.3 | 9-14 ms | 64-79 thousand | 100 s, 5813 frames |
| 60, at 1600x720 (100%) | 59.3 | 11-14 ms | 52-82 thousand | 54 s, 3119 frames |

- The time per frame covers the game's CPU work, submitting the frame and
  waiting for the previous frame's fence, so at 60 fps there are still a few
  milliseconds to spare. At the full 1600x720 it grows by only about 2 ms
  and the game still holds 59 fps; the view also filled the whole screen
  once the activity used the area beside the camera cutout.
- The world (64 chunks, about 62 thousand faces) is generated and meshed in
  about 260 ms through Box64.
- Memory peaks: launcher 152-158 MB, VirGL server 59 MB, game 33-35 MB, about
  250 MB in total. The lowest available memory stayed above 870 MB.
- The server's "failed to export fence sync object" message from the
  performance test did not appear: the game never calls glFinish and waits on
  fences instead.

### Memory on the test phone (November 2026)

From the in-app memory report (**Settings > Memory report**) on the Galaxy
M12 (3704 MB visible to apps), idle, with and without power saving:

| Power saving | Available | Free | Cached | Swap used (of 4095 MB) |
|--------------|-----------|------|--------|------------------------|
| On           | 797–832 MB | 207–272 MB | 657–758 MB | ~1640 MB |
| Off          | 904–1233 MB | 81–136 MB | 886–1263 MB | ~1660 MB |

- The low "free" number is normal: Android keeps memory full of cache and
  frees it when needed. "Available" is the number that matters.
- Power saving changes little (it limits CPU and background work, not RAM).
- About 1.6 GB is already in swap (zram plus Samsung's RAM Plus): the system
  and the other installed apps want much more than the phone's RAM. That,
  not a fault, is why so little is available.

During the graphics performance test (power saving on), available memory
went from 929 MB to a lowest of 561 MB. Peaks: the app 164 MB, the VirGL
server 91 MB, Box64 with the test program 37 MB, about 295 MB in total; the
rest of the drop is the GPU driver's memory and cache. The heavy steps were
slower than in earlier runs (61–70 ms instead of 40–60 ms), probably from
power saving or heat.

What it means for the game: with the phone idle, roughly 0.8–1.2 GB is
available for the game client, our runtime and the GPU, plus swap (which is
slow). The launcher should keep its own share small while the game runs
(release the UI, run the server lean) and suggest closing other apps.

### Consequence for the CPU choice

To run glibc Linux programs on Android there are three known setups:

| Setup | How it works | GPU driver access | Concerns |
|-------|--------------|-------------------|----------|
| Box64 built for glibc + an ARM64 glibc shipped in the app | Box64 runs as a Linux program on its own glibc; wrapped libraries call native glibc libraries | The phone's GL/Vulkan driver is a bionic library and cannot be loaded from a glibc process. Works on Adreno with Mesa Turnip (talks to the kernel directly). On Mali it needs a bridge such as VirGL to a separate process, which is slow | Bad fit for Mali phones like the main test device |
| FEX in a bionic process + an x86-64 glibc rootfs | The guest's own x86-64 glibc is translated; calls to GL are forwarded to the Android driver with thunks | Direct, through the vendor GLES driver | More translated code (no wrapped libc). This is the setup Vexa uses and it reached the game's startup |
| Patch Box64's Android build to accept glibc programs | Enable `__libc_start_main` next to `__libc_init` | Direct | Box64's bionic wrapped libc was not written for glibc programs; differences in structures and behavior are likely. We would carry patches upstream does not support |

### FEX on Android: first build attempt (November 2026)

- FEX-2611 refuses to configure for Android: `Unsupported system type
  Android. FEX only supports Linux and Windows.`
- Forcing a Linux-style build with the NDK 29 compiler gets most of the
  code to compile; 14 files fail. The errors are gaps between glibc and
  Android's bionic libc (`shm_open`, `memfd_create`, `getdents64`,
  `valloc`, `ELF32_ST_VISIBILITY`) and a libc++ `std::function` limitation.
  These are only the build errors: signals, memory and syscall handling on
  Android would come next, and Vexa's notes call that area
  correctness-sensitive.
- In short, running FEX on Android means maintaining our own Android port
  of FEX. Vexa has already done this work, but its code has no license.

### Upstream AI policies

- **FEX** (`AGENTS.md`): "AI must not be used to generate code for
  contributions to this project."
- **Box64** (`AGENTS.md`, `CONTRIBUTING.md`): does not accept pull requests
  that are fully or predominantly AI-generated; AI use must be disclosed.
- **Vexa** (`docs/AI_USAGE_POLICY.md`): AI must not own threading, signal,
  memory or low-level runtime behavior.

Using these projects as dependencies is fine (MIT). But any patch we send
upstream must be written by a human contributor, and AI-written patches we
keep in this repository must be fully understood by whoever submits them
(see CONTRIBUTING.md).


1. **CPU translator proof of concept.** Add Box64 (and later FEX) as pinned
   submodules, build them with the NDK, and run our own x86-64 "hello world"
   (built from source in this repository) inside the app. Measure startup
   time and memory.
2. **Guest rootfs.** Download and verify a minimal x86-64 glibc environment,
   with a preflight check in the app.
3. **Graphics.** Build MobileGlues, forward guest GL calls to it, and render
   a small open-source GL 4.1 test program to a `SurfaceView`.
4. **Audio and input.** OpenAL Soft and SDL bridging, tested with small
   open-source programs.
5. **The game.** Only after the account work in `:core` is done: download
   the client with the player's account, pass the official session to it,
   and start it.

Steps 1 to 4 never need game files, so anyone can develop and test them.

## Sources

- Official requirements: <https://hytale.com/download> and
  <https://hytale.com/news/2025/12/hytale-hardware-requirements>
- Vexa repository and its `docs/` and `rootfs_required.md` (read November 2026)
- License files and READMEs of each project linked above
