# Changelog

All notable changes to VSL Finesse (formerly VSL Performer) are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html). VSL Finesse is in **alpha**: while the version is below 1.0.0, any release may change behaviour, settings or file formats. The minor version goes up for new features and behaviour changes, the patch version for fixes.

## [Unreleased]

### Added
- **Crossfaded articulations are shaped like a player would.** Some articulations blend two sounds with a controller (Synchron's Dimension Controls): Marcato adds a staccato attack on top of the sustain, XF tremolo turns the sustain into tremolo. The app reads which kind each one is from the loaded preset and shapes it: Marcato's attack gets more bite on strong beats, phrase starts, after rests, on leaps, in louder passages and on notes your file accents, and less in fast notes; XF tremolo leans into the tremolo towards each phrase's peak and back. Character sets how far they range. They're written wherever the preset sends the control (velocity, CC1 or CC20), never over the app's own dynamics or a controller you kept from your file. The Report lists what was shaped.
- **Settings** (⌘,) with a choice for the controller data (CCs) already in MIDI files: remove it **Always**, **Never**, or **Ask** when a file has some (the default). Until now the app always removed it, including controllers it doesn't write itself, such as CC20 (Synchron's tremolo crossfade) or CC64 (sustain pedal). If you keep them, the app's own dynamics (CC1) and expression (CC11) still replace the file's where it writes them, since that's the bulk of what the app does; untick **Replace CC1 and CC11 with mine** and a CC1 or CC11 line in your file wins instead (the app writes none for that controller). The Report says how many of the file's controller events were kept and removed.
- **Articulation picker:** drag across some bars (or select nothing for the whole part), click **Articulation** in the strip above the notes and pick one by name, e.g. **Long notes > Marcato**. The names come from the preset loaded in Synchron Player. The app writes that articulation's keyswitches just before the passage and switches back afterwards to what was playing before. Keyswitches in your file inside the passage are replaced; the rest stay as written. Each preset has its own articulations, levels and keys (e.g. Violin I vs Violin I PLUS, VelXF sus vs Velocity), so the menu always shows the loaded preset's, nested like its tree. A pick is remembered by name: after loading another preset it uses that preset's keys, and if that preset doesn't have it, it's shown in orange and writes nothing. Picks show in a lane above the dynamics markings (right-click one to remove it), are saved in sessions, and go into the exported MIDI file.
- With a Synchron preset loaded, each articulation's dynamics follow what the preset says rather than the note's length: in VelXF sus, Staccato and Detache follow velocity even when long and Sustain crossfades with CC1 even when short; on timpani, hits follow velocity and rolls crossfade. The **Velocity** preset folder still means velocity for every note.
- Presets outside the three preset folders, such as Harp I or Timpani VelXF rolls, now set the preset folder too: Velocity for presets whose layers follow velocity (Harp), VelXF sus otherwise.
- Keyswitches in your file are now read by the loaded Synchron preset's own levels, so presets whose articulation keys don't fit one octave per level are understood correctly.
- The Articulations list names each articulation, e.g. "Long notes > Marcato (C#0 + C#1)", using the Synchron preset's names, and the Report says how many keyswitches were written for picked articulations.
- A welcome screen the first time the app starts: what the app is built for (Synchron Player), and why macOS asks for each permission (removable volumes, Documents, local network, keystrokes). Open it again from **Help → Welcome and Permissions...**. If Synchron Player isn't installed, it says so.
- macOS's own permission prompts now say why VSL Finesse is asking.
- A warning when Synchron Player isn't installed (when a MIDI file is opened) and when you load another instrument: the app was designed for Synchron Player, and other virtual instruments may not behave as intended. Each warning appears once per run.
- When you load a preset in Synchron Player, the track's **instrument profile** (and with it the playable range) and **Synchron preset folder** now follow it: e.g. "02 VelXF - MOD/01 Violin I - VelXF" picks Violins and VelXF - MOD. The app checks while Synchron's window is open and when you close it. Only a change of preset does this, so a profile you pick by hand stays until you load another preset. Presets the app doesn't recognise (no matching profile) leave the profile as it is.
- VSL Finesse is now free software under the GNU AGPLv3 (see `LICENSE.md`), the licence that JUCE, which the app is built with, requires for open-source apps.

### Changed
- In the VelXF presets, Marcato velocities are no longer kept exactly as written: velocity sets Marcato's attack there, so the app now shapes it (your file's accents still count). Other notes' velocities stay as written, as before.
- Opening a MIDI file on a track with no instrument now loads Synchron Player automatically, if it's installed. Click **Edit** to open it and load your preset. You can still pick another instrument from the track's instrument button.
- Synchron Player now opens in its **Precision** view instead of Flow when the app loads it. The app's preset folder choices (VelXF sus - MOD, VelXF - MOD, Velocity) are Precision presets. A Flow preset you've loaded yourself is left alone, and so is a track restored from a session.
- The **Plain** character button is now called **Simple**.

### Fixed
- Dynamics markings saved in a session are restored when you open it (they were dropped).
- A MIDI file named after a longer instrument name, such as "Bass Clarinet.mid", no longer gets the shorter one's profile (Clarinet).

## [0.4.0] - 2026-11-07

### Changed
- The app is now called **VSL Finesse** (it was VSL Performer). The download is **VSL-Finesse-macOS** and the app is **VSL Finesse.app**; delete the old VSL Performer.app. macOS treats it as a new app, so it may ask again for permissions such as audio or file access.
- The command line tool is now `vsl-finesse` (it was `vslp-finesse`).
- Sessions are saved as `.vslfsession` files. Older `.vslpsession` files still open.
- Your saved instrument profiles and settings move to `~/Library/Application Support/VSL Finesse` the first time the new app starts.

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

[Unreleased]: https://github.com/mintindeed/vsl-finesse/compare/main...HEAD
[0.4.0]: https://github.com/mintindeed/vsl-finesse/commits/main
[0.3.1]: https://github.com/mintindeed/vsl-finesse/commit/9d58758
[0.3.0]: https://github.com/mintindeed/vsl-finesse/commit/e014f4e
[0.2.0]: https://github.com/mintindeed/vsl-finesse/commit/b9d4962
[0.1.1]: https://github.com/mintindeed/vsl-finesse/commit/a21ecf6
[0.1.0]: https://github.com/mintindeed/vsl-finesse/commit/18f1837
