# Working on VSL Finesse

## Changelog and versioning (required)

- Every change that affects users (new feature, enhancement, bug fix, behaviour change, removal) gets an entry in `CHANGELOG.md` in the same commit. Follow the Keep a Changelog headings: Added, Changed, Deprecated, Removed, Fixed, Security. Write entries for the user (what changed for them), not for developers.
- Versions follow semantic versioning. The app is in alpha, so versions stay below 1.0.0: bump the **minor** version (0.x.0) for new features or behaviour changes, the **patch** version (0.x.y) for fixes only.
- When a change is released (pushed to `main` for the user to try), move the `[Unreleased]` entries under a new version heading with the date, bump `VERSION` in the top-level `CMakeLists.txt` to match, and update the links at the bottom of `CHANGELOG.md`.
- Pure refactors, test-only or CI-only changes don't need an entry unless they change what users get.
- Released entries are history: don't rewrite them when the docs move or names change.

## Docs

Each document has one reader:

- `README.md` (Standard Readme): for a musician. What the app is and is for, what it works with, how to get it running, how to use it. No implementation detail.
- `docs/METHODOLOGY.md` (Diátaxis explanation): how the app performs, with the numbers.
- `docs/DEVELOPMENT.md` (Diátaxis how-to and reference): building, testing, architecture, file formats, Synchron Player's state, CI, versioning, signed builds.
- `CONTRIBUTING.md`: the app's intent and principles (the design decisions), what contributions are welcome, how to contribute, and the roadmap.

### Where each change goes

Before committing, check every place listed for the kind of change you made. Several facts deliberately live in two places (a doc and the app's own text, or a rule for agents and the same rule for people); those pairs are marked **both**.

| You changed | Update |
|---|---|
| How the app performs (any engine behaviour or constant) | `docs/METHODOLOGY.md`: the section and its numbers, and the "Step by step" table if a stage changed. The comment at the top of the engine file. `CHANGELOG.md`. `README.md` only if what the user does or sees changes. |
| What the user does or sees (UI, menus, settings, dialogs, tooltips) | `README.md` Usage, and **both** the app's own text that says the same thing: `WelcomeOverlay.cpp`, the Settings window in `MainComponent.cpp`, dialogs in `AppContext.cpp`, tooltips. `CHANGELOG.md`. |
| macOS permissions | **All of**: README "Permissions macOS asks for", `WelcomeOverlay.cpp`, the Info.plist purpose strings (`PLIST_TO_MERGE` in `app/CMakeLists.txt`), DEVELOPMENT "macOS permissions". |
| Something learned about Synchron Player (a new preset dump) | A fixture in `tests/SynchronTrees.h` and a test, **both** DEVELOPMENT "Synchron Player's saved state" (including the presets table), and METHODOLOGY if it changes what the app does. If it changes what a tester has to do or know (when the state is read, what's captured at preset load, the dump format), DEVELOPMENT "Testing with your own library" and `scripts/synchron-state.py`. |
| Something tried on a Mac with Synchron Player for the first time | Remove it from the "Never tried" list in DEVELOPMENT "Testing with your own library". |
| What the app has been tried with (VSL products, DAWs) or a known gap | README "What it works with, and where help is welcome" only; CONTRIBUTING links to it. |
| Command line options | **both** `usage()` in `cli/Main.cpp` and DEVELOPMENT "Command line tool" (kept identical). |
| Code structure, files, build, file formats | `docs/DEVELOPMENT.md` (Project layout, Architecture, the format sections). |
| A design decision or principle | CONTRIBUTING "Principles". |
| A planned feature, or one that shipped | CONTRIBUTING "Roadmap": add it, or remove it once built (it's then documented in README and METHODOLOGY). |
| Versioning, changelog or CI rules | **both** this file and DEVELOPMENT ("Versions and the changelog", "Continuous integration"). CONTRIBUTING only links to them. |
| How contributors and their agents are expected to work | **both** this file and CONTRIBUTING "AI-assisted development". |

### Working attentively

This file is for every agent working on the repository (`AGENTS.md` is a link to it). CONTRIBUTING "AI-assisted development" says the same to the people directing them.

- Go through the "Where each change goes" table before every commit.
- Check facts instead of recalling them: VSL's product names, note names, what a Synchron preset does, what a library contains. Check against the code, a preset's saved state, a test fixture or VSL's own documentation, and say where a claim comes from. A product name and a note name stated from memory have both been wrong before.
- When you learn something future work needs (a new pair of places that must change together, a name, a pitfall), add it to this file in the same commit.

### Names

- VSL's products: Vienna **Synchron Player**; the **Studio Special Edition** (Vol. 1 and Vol. 2, with their PLUS articulations; Synchron Player's preset folder is "Studio Special Editions"), which the author owns and the app is made for; the free **Big Bang Orchestra** collections. Don't confuse the Studio Special Edition with the earlier **SYNCHRON-ized Special Edition**, a different, untested product.
- Synchron's terms: Dimension Tree, dimension, Dimension Control (Dim.Ctrl/A-H), Mix and XFade slots; in VSL Finesse's own words, "crossfaded articulations".
- Note names follow Logic (C3 = middle C, MIDI 60). Synchron Player calls middle C "C4", so its C1 is Logic's C0. Check a note name against `vslf::noteName` before writing it in a doc.
- British spelling (licence, behaviour, colour).

## Build and test

- `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
- The engine (`engine/`) has no dependencies; add a unit test in `tests/` for every engine behaviour change.

## GitHub Actions minutes

The repository is private, so Actions minutes are billed, and macOS minutes count ten times. Builds run on pull requests only (not on plain pushes or merges to `main`), and every push to a branch with an open pull request starts one. Build and test locally first, batch related changes into one push, and don't push just to re-trigger a build. Docs-only changes (`*.md`, `docs/`) don't start a build, and a newer push cancels a running build on the same pull request.
