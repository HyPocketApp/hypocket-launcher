# Contributing to HyPocket Launcher

Thanks for your interest in HyPocket Launcher, an open-source launcher for
playing Hytale on Android, focused on low-end phones (little RAM, weak GPU).

HyPocket is an **unofficial** project. It is not affiliated with, endorsed by
or sponsored by Hypixel Studios.

Please read this whole guide before opening an issue or a pull request.

## Project rules

These rules are not negotiable. Pull requests that break them will be closed.

1. **An original account and a legitimate copy of the game are required.**
   Players sign in with their own official account.
2. **Never distribute game files.** Do not commit, upload, attach or link to
   game binaries, assets or any other game files. The launcher downloads them
   using the player's own account.
3. **Account verification lives in the core of the app.** It must not be a
   setting, flag, build variant or anything else that is easy to turn off.
4. **No piracy.** We do not accept code, issues or discussions that help
   bypass account verification or build a cracked/pirated version.
5. **Check licenses before reusing code.** Before copying or adapting code
   from another project, confirm its license is compatible with GPL-3.0 and
   keep its copyright notice. Mention the source and license in the PR.
6. **Naming.** "Hytale" is never part of the project name, only of its
   description ("for Hytale"). Forks must not use the name "HyPocket".
7. **Releases.** Official APKs are published only through the GitHub releases
   of the [HyPocketApp](https://github.com/HyPocketApp) organization. Only
   this repository is the official one.

## License

The project is licensed under the [GNU GPL-3.0](LICENSE). By contributing,
you agree that your contribution is licensed under the same terms.

## Architecture

The code is split into two parts that must stay separate:

- **Kotlin**: UI and app logic (screens, sign-in, downloads, settings).
- **C/C++ via the Android NDK**: the engine (x86-64 to ARM instruction
  translation and the graphics translation layers).

The planned Gradle modules are:

| Module    | Language | Responsibility                                          |
|-----------|----------|---------------------------------------------------------|
| `:app`    | Kotlin   | Screens, navigation, view models                        |
| `:core`   | Kotlin   | Account, verification, downloads, settings storage      |
| `:engine` | C/C++    | Native engine, exposed to Kotlin through a thin JNI API |

Rules:

- No UI logic in the engine, and no engine logic in the UI.
- Kotlin talks to native code only through the JNI bridge in `:engine`.
- Account verification belongs to `:core`, and the engine is only started
  after verification succeeds.

## Getting started

### Requirements

- Android Studio (latest stable)
- JDK 17 or newer
- Android SDK Platform 36
- Android NDK 28.2.13676358 and CMake 3.31.6 (install both from the
  Android Studio SDK Manager, under "SDK Tools"; the versions are pinned in
  `engine/build.gradle.kts`)

If you build from the command line, create a `local.properties` file with
`sdk.dir=/path/to/your/Android/sdk` (Android Studio does this for you).
- An Android device or emulator. For engine work, a real ARM64 device is
  strongly recommended, ideally a low-end one.

### Build and run

```bash
git clone https://github.com/HyPocketApp/hypocket-launcher.git
cd hypocket-launcher
./gradlew assembleDebug        # build a debug APK
./gradlew installDebug         # install it on a connected device
```

### Test

```bash
./gradlew test                 # unit tests
./gradlew connectedCheck       # instrumented tests (needs a device)
./gradlew lint                 # Android lint
```

Run the tests and lint before opening a pull request.

## Language

- Code, comments, identifiers, commit messages and pull requests: **English**.
- App text goes through Android string resources only, never hard-coded:
  - `res/values/strings.xml`: English (default)
  - `res/values-pt-rBR/strings.xml`: Brazilian Portuguese
- When you add or change a string, update every language you can. If you
  cannot translate it, add it in English and say so in the PR.
- We do not use per-language branches or repositories.
- Documentation: `README.md` (English) and `README.pt-BR.md` (Portuguese) must
  be kept in sync.

## Commit messages

We use [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<optional scope>): <short summary in the imperative>

<optional body explaining what and why>
```

Common types: `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `build`,
`ci`, `chore`. Common scopes: `app`, `core`, `engine`, `i18n`.

Examples:

```
feat(core): add download progress reporting
fix(engine): handle missing Vulkan extension on Mali GPUs
docs: update build instructions
```

Keep commits focused: one logical change per commit.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Make your change, following the rules and conventions above.
3. Build, run the tests and lint, and test on a device when the change
   affects behavior.
4. Open a pull request describing what changed, why, and how you tested it
   (device model, Android version, RAM and GPU when relevant).

Large changes (new modules, new translation layers, new dependencies) should
start as an issue so we can discuss them first.

## Using AI tools

You are welcome to use AI tools (code assistants, chatbots, agents) to help
you contribute. However, **you are responsible for every pull request you
submit**, whoever or whatever wrote the code:

- You must understand the code in your PR and be able to explain it in review.
- You must have built and tested it yourself.
- You must check that it follows the project rules, especially the license
  rule: AI-generated code can reproduce code from projects with incompatible
  licenses.

Pull requests that show the author has not read or tested their own code may
be closed without review.

## Reporting security issues

Please do not open public issues for security problems. Contact the
maintainers privately through the GitHub security advisory feature of this
repository.
