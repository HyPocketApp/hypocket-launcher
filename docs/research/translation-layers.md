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
