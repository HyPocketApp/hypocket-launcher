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
