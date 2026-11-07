# Changelog

All notable changes to VSL Performer are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html). VSL Performer is in **alpha**: while the version is below 1.0.0, any release may change behaviour, settings or file formats. The minor version goes up for new features and behaviour changes, the patch version for fixes.

## [Unreleased]

## [0.3.1] - 2026-11-07

### Added
- Signed and notarized macOS builds: once the Developer ID secrets are added to the repository (see README, "Signed builds"), downloaded builds open with a double-click, with no trip to System Settings > Privacy & Security. Without the secrets, builds stay unsigned as before.

## [0.3.0] - 2026-11-07

### Added
- Dynamics strip above the piano roll: **pp p mp mf f ff**, **cresc.** and **dim.** buttons. They apply to the bars you drag across in the piano roll, or to the whole part when nothing is selected. Each button's tooltip spells out the marking (e.g. "pp: pianissimo, very soft").
- Markings lane above the notes showing dynamics and hairpins. Right-click a marking to remove it; **Clear** removes the markings in the selection (or all of them). Markings are saved in sessions.
- A new dynamic replaces the part of an older one it covers. A crescendo or diminuendo grows two dynamic steps across its passage and keeps that level until the next dynamic marking.
- **Character** buttons: Plain, Expressive, Very expressive.
- **Human pauses** (on by default): breaths in long wind and brass phrases, time for the bow to jump over a string (strings are worked out per note from the instrument's open strings), and a short lift where a phrase ends without a rest. Pauses come out of the earlier note, so the beat never moves.
- Open strings for the Violins, Violas, Cellos and Basses profiles (`"strings"` in profile JSON).
- The Report lists breaths and other human pauses.
- README section on how the app performs, including the KTH performance rules it uses.
- This changelog.

### Changed
- The performance sliders moved into a closed **Fine-tune** section.
- Clicking in the piano roll moves the play position; dragging selects bars. Drag in the bar-number ruler to scrub.

## [0.2.0] - 2026-11-06

### Added
- **Synchron preset folder** choice per track (VelXF sus - MOD, VelXF - MOD, Velocity). The app remembers the last choice for new tracks.
- **Save instrument details to Desktop** in the instrument menu: writes the plug-in's parameters, programs and state to a JSON file, to learn how Synchron Player stores its articulation tree.
- `--type` option for the `vslp-finesse` command line tool.

### Changed
- The patch type is one choice per track instead of a setting per articulation.
- VelXF sus - MOD: long notes crossfade with CC1, short notes follow velocity.
- VelXF - MOD: velocities are left as written, because they can select an attack variant (e.g. Marcato).
- Velocity: no CC1 is written; long notes are shaped with CC11.
- Connected notes always overlap slightly; the per-articulation Legato option is gone.
- Keyswitch levels default to one octave per level, like the Synchron presets (e.g. C1 + C#2 is one articulation).
- The Articulations panel is now a read-only legend of the keyswitches in the file.

### Removed
- Unmapped-keyswitch warnings.

## [0.1.1] - 2026-11-06

### Fixed
- The app crashed while scanning for plug-ins when a third-party Audio Unit (Nektarine) misbehaved. On macOS the instrument menu now lists Audio Unit instruments without loading them, and loads only the one you pick. Vienna/Synchron instruments are listed first.
- macOS build failed with CMake 4 (the test library is now used header-only).

## [0.1.0] - 2026-11-06

### Added
- First alpha: the performance engine (keyswitch-preserving clean-up, phrase analysis, CC1/CC11 curves, velocities, legato overlaps, humanized timing, wind/brass breath limits).
- Mac app: drag-and-drop MIDI, tracks, Synchron Player hosting (AU/VST3), Original/Finessed A/B playback, piano roll with CC lanes, export, sessions.
- `vslp-finesse` command line tool.
- Built-in instrument profiles for common orchestral instruments.
- Unit tests and a GitHub Actions build that produces a downloadable macOS app.

[Unreleased]: https://github.com/mintindeed/vsl-performer/compare/main...HEAD
[0.3.1]: https://github.com/mintindeed/vsl-performer/commits/main
[0.3.0]: https://github.com/mintindeed/vsl-performer/commit/e014f4e
[0.2.0]: https://github.com/mintindeed/vsl-performer/commit/b9d4962
[0.1.1]: https://github.com/mintindeed/vsl-performer/commit/a21ecf6
[0.1.0]: https://github.com/mintindeed/vsl-performer/commit/18f1837
