# Working on VSL Finesse

## Changelog and versioning (required)

- Every change that affects users (new feature, enhancement, bug fix, behaviour change, removal) gets an entry in `CHANGELOG.md` in the same commit. Follow the Keep a Changelog headings: Added, Changed, Deprecated, Removed, Fixed, Security. Write entries for the user (what changed for them), not for developers.
- Versions follow semantic versioning. The app is in alpha, so versions stay below 1.0.0: bump the **minor** version (0.x.0) for new features or behaviour changes, the **patch** version (0.x.y) for fixes only.
- When a change is released (pushed to `main` for the user to try), move the `[Unreleased]` entries under a new version heading with the date, bump `VERSION` in the top-level `CMakeLists.txt` to match, and update the links at the bottom of `CHANGELOG.md`.
- Pure refactors, test-only or CI-only changes don't need an entry unless they change what users get.

## Docs

- Keep `README.md` in step with the app: how to use it, and how it performs (the KTH rules and human touches section).
- Design decisions go in `docs/DESIGN.md`.

## Build and test

- `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
- The engine (`engine/`) has no dependencies; add a unit test in `tests/` for every engine behaviour change.

## GitHub Actions minutes

The repository is private, so Actions minutes are billed, and macOS minutes count ten times. Build and test locally first, batch related changes into one push, and don't push just to re-trigger a build. Docs-only changes (`*.md`, `docs/`) don't start a build, and a newer push cancels a running build on the same branch.
