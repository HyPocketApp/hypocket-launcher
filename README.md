**English** | [Português (Brasil)](README.pt-BR.md)

# HyPocket Launcher

An open-source launcher for playing Hytale on Android, focused on low-end
phones (little RAM, weak GPU).

> [!IMPORTANT]
> **Unofficial project.** HyPocket is not affiliated with, endorsed by or
> sponsored by Hypixel Studios. Hytale is a trademark of its respective owner.

## Status

HyPocket is in early development and **not playable yet**. There are no
releases so far.

## Requirements

- **An original account and a legitimate copy of the game.** You sign in with
  your own official account.
- An Android phone with an ARM64 processor.

HyPocket does **not** include or distribute any game files. The launcher
downloads them using your own account. There is no offline or pirated mode,
and there never will be.

## How it works

The Hytale client is built for PCs (x86-64 processors and desktop graphics
APIs). To run it on a phone, HyPocket needs to:

1. Translate x86-64 instructions to ARM (with a translator such as Box64 or
   FEX).
2. Translate desktop graphics calls to OpenGL ES or Vulkan (with a layer such
   as Zink or gl4es).
3. Keep memory use and GPU load low enough for low-end phones.

The app itself (sign-in, downloads, settings) is written in Kotlin. The engine
(translation layers) is written in C/C++ with the Android NDK.

## Roadmap

- [x] Project rules and contribution guide
- [ ] Android app skeleton: sign-in, game download, settings
- [ ] Research and integrate the CPU and graphics translation layers
- [ ] Performance work for low-end devices
- [ ] First public release

## Downloads

Official APKs will be published **only** on the
[releases page](https://github.com/HyPocketApp/hypocket-launcher/releases) of
this repository. Do not install HyPocket from other sources.

## Official repository and name policy

[HyPocketApp/hypocket-launcher](https://github.com/HyPocketApp/hypocket-launcher)
is the **only** official repository of HyPocket Launcher.

You are free to fork this project under the terms of the GPL-3.0, but **forks
must not use the name "HyPocket"** or present themselves as the official
project. Please choose a different name for your fork.

## Contributing

Contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) for how to
build and test the project, the commit conventions and the project rules.

## License

HyPocket Launcher is licensed under the
[GNU General Public License v3.0](LICENSE). Any distributed fork or
modification must also be released as open source under the same license.
