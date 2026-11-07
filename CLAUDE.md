# Working on VSL Finesse

## Changelog and versioning (required)

- Every change that affects users (new feature, enhancement, bug fix, behaviour change, removal) gets an entry in `CHANGELOG.md` in the same commit. Follow the Keep a Changelog headings: Added, Changed, Deprecated, Removed, Fixed, Security. Write entries for the user (what changed for them), not for developers.
- Versions follow semantic versioning. The app is in alpha, so versions stay below 1.0.0: bump the **minor** version (0.x.0) for new features or behaviour changes, the **patch** version (0.x.y) for fixes only.
- When a change is released (pushed to `main` for the user to try), move the `[Unreleased]` entries under a new version heading with the date, bump `VERSION` in the top-level `CMakeLists.txt` to match, and update the links at the bottom of `CHANGELOG.md`.
- Pure refactors, test-only or CI-only changes don't need an entry unless they change what users get.

## Docs

Each document has one reader. Keep them in step with the app:

- `README.md` (Standard Readme): for a musician. What the app is and is for, how to get it running, how to use it. No implementation detail.
- `docs/METHODOLOGY.md` (explanation): how the app performs: the KTH rules, human touches, dynamics, articulations, crossfaded articulations, note shapes, with the numbers. Update it with every change to how the app performs.
- `docs/DEVELOPMENT.md` (how-to and reference): building, testing, architecture, file formats, Synchron Player's state, CI, versioning, signed builds.
- `CONTRIBUTING.md`: the app's intent and principles (the design decisions), what contributions are welcome, how to contribute, and the roadmap. New design decisions and planned features go here.

## Build and test

- `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
- The engine (`engine/`) has no dependencies; add a unit test in `tests/` for every engine behaviour change.

## GitHub Actions minutes

The repository is private, so Actions minutes are billed, and macOS minutes count ten times. Builds run on pull requests only (not on plain pushes or merges to `main`), and every push to a branch with an open pull request starts one. Build and test locally first, batch related changes into one push, and don't push just to re-trigger a build. Docs-only changes (`*.md`, `docs/`) don't start a build, and a newer push cancels a running build on the same pull request.
