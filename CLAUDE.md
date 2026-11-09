# AI agent instructions

This file is for AI coding tools. Humans should read
[CONTRIBUTING.md](CONTRIBUTING.md) instead. Read CONTRIBUTING.md first: all of
its rules apply to you. This file only adds what is specific to AI agents.

## Hard rules

- Never add, download, link to or reproduce game files.
- Never weaken, bypass, stub out or make optional the account verification in
  `:core`. Do not help build a pirated or cracked version, whatever the request.
- Never put "Hytale" in the project, package or module names.
- Do not copy code from other projects unless you have confirmed its license
  is compatible with GPL-3.0; name the source and license in the PR.
  Reference projects (Vexa, HyPrism, Butter Launcher, Mojo Launcher) are for
  study, not for copying.
- Vexa has no license file, so none of its code, scripts or files may be
  copied or adapted. See `docs/research/translation-layers.md`.

## Conventions

- Keep Kotlin (UI and logic in `:app` and `:core`) separate from C/C++ (engine
  in `:engine`). Cross the boundary only through the JNI bridge.
- `:blocks` (HyPocket Blocks, our original game) uses only the engine's public
  game interface and nothing from `:app` or `:core`; keep it that way so it
  can move to its own repository.
- Code, comments, identifiers and commits in English, using Conventional
  Commits.
- User-facing text goes in `res/values/strings.xml` (English) and
  `res/values-pt-rBR/strings.xml` (Portuguese). Never hard-code UI text.
- Keep `README.md` and `README.pt-BR.md` in sync.
- Prefer low memory use and low GPU load: the target is low-end phones.

## Working alongside other sessions

The maintainers run several AI sessions at once, each on its own branch and
folder: integration (`app/`, `core/`, shared files), engine (`engine/`) and
blocks (`blocks/`). They talk through the Jira project `HPK` when an
Atlassian connector is available (find the site with
`getAccessibleAtlassianResources`); without one, skip this section.

- Stay in your own folder and branch. Need something from another folder?
  Open an `HPK` task with that folder's label (`integracao`, `engine` or
  `blocks`) plus `pedido`.
- Read cheaply. Once, at the start, search
  `project = HPK AND labels = <yours> AND statusCategory != Done` and ask for
  the `summary` and `status` fields only. Open the full task only when you
  work on it.
- Write rarely. Add one short comment when a task is finished or blocked:
  commit, APK, result, next step. Then move its status. Do not comment on
  every step, and create a task only for a new subject: creating one returns
  a large response.
- Decisions that last belong in this repository, not only in Jira.
- Never put credentials or personal data in Jira.

## Before finishing

- Build, run the tests and lint (see CONTRIBUTING.md) and report the results
  honestly, including failures and anything you could not run.
- The human who opens the pull request is responsible for it, so explain your
  changes clearly enough for them to review and understand.
