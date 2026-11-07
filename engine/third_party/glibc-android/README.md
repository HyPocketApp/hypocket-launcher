# glibc for Android

Box64 runs as a Linux (glibc) program inside the app. Stock glibc cannot run
there: Android's app seccomp filter kills the process when it calls
`set_robust_list`, `rseq`, `clone3`, `faccessat2` or `fchmodat2`, which
Android 13 does not allow for apps. glibc calls `set_robust_list` at startup.

`android-seccomp.patch` makes glibc 2.39 behave as on a kernel without those
system calls:

| System call | Change |
|-------------|--------|
| `set_robust_list` | Never called; robust mutexes report as unsupported |
| `rseq` | Off by default (tunable `glibc.pthread.rseq` defaults to 0) |
| `clone3` | Not used; glibc uses `clone` |
| `faccessat2`, `fchmodat2` | Not used; glibc uses its existing fallbacks |

`build.sh` downloads the official glibc 2.39 release, checks its SHA-256,
applies the patch and builds it with an aarch64 cross compiler.

Status: the patch is written but **not built or tested yet**.

License: glibc is LGPL-2.1-or-later. This patch is under the same license.
A public release of HyPocket must offer the glibc source and this patch.
