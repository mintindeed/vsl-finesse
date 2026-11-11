# Developing VSL Finesse

Technical documentation for working on the app: building, testing, how the code fits together, the file formats it reads and writes, and releasing. For what the app does musically and why, see [METHODOLOGY.md](METHODOLOGY.md); for the project's principles and roadmap, [CONTRIBUTING.md](../CONTRIBUTING.md).

## Contents

- [Building](#building)
- [Testing](#testing)
- [Testing with your own library](#testing-with-your-own-library)
- [Command line tool](#command-line-tool)
- [Project layout](#project-layout)
- [Architecture](#architecture)
- [Instrument profiles](#instrument-profiles)
- [Synchron Player's saved state](#synchron-players-saved-state)
- [How the app uses Synchron's state](#how-the-app-uses-synchrons-state)
- [macOS permissions](#macos-permissions)
- [Versions and the changelog](#versions-and-the-changelog)
- [Continuous integration](#continuous-integration)
- [Signed builds](#signed-builds)

## Building

**On a Mac** (needs the Xcode Command Line Tools and CMake):

```bash
xcode-select --install     # once
brew install cmake         # once
./scripts/build-mac.sh --open
```

The script configures, builds and runs the tests, then opens `build/app/VSLFinesseApp_artefacts/Release/VSL Finesse.app`.

**With CMake directly** (any platform; JUCE, Zstandard and doctest are downloaded by CMake):

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Options: `VSLF_BUILD_APP`, `VSLF_BUILD_CLI` and `VSLF_BUILD_TESTS` (all on). The app is made for macOS but builds on Linux too, which is handy for a quick look at the UI; there it needs JUCE's Linux dependencies (ALSA, X11, FreeType, fontconfig: see the `apt-get` line in [`.github/workflows/build.yml`](../.github/workflows/build.yml)). With `-DVSLF_BUILD_APP=OFF` only the engine, the command line tool and the tests are built.

## Testing

`ctest` runs `vslf-tests` (doctest):
- `tests/EngineTests.cpp`: the engine, on parts built in code (`tests/TestHelpers.h`). Every engine behaviour change gets a test here.
- `tests/IoTests.cpp`: MIDI files, JSON, Synchron Player's state.
- `tests/SynchronTrees.h`: the articulation trees of real Special Edition presets, cut down to the fields the app reads. Add one whenever a new preset shows something new.

**Getting a preset's state** for a fixture: see [Testing with your own library](#testing-with-your-own-library). `scripts/synchron-state.py <details file> --json` prints the whole state; cut it down to the fields the reader uses before adding it as a fixture (the trees in `tests/SynchronTrees.h` show which). In C++, `MemoryBlock::fromBase64Encoding()` and `vslf::io::synchronStateJson()` do the same.

## Testing with your own library

How to try VSL Finesse with a VSL library, preset or DAW it hasn't been tried with, and what to send back. [README](../README.md#what-it-works-with-and-where-help-is-welcome) lists what has been tried.

### Before you start

- **Use the Audio Unit, on a Mac.** The app reads Synchron Player's saved state only in the Audio Unit's format. On macOS the instrument menu lists Audio Unit instruments only, and loads nothing until you pick one (a full plug-in scan once crashed the app on a third-party plug-in). The VST3 version, which other platforms use, plays, but the app can't read its state: no preset following, no articulation menu, none of the per-preset shaping.
- **Use Precision presets.** The app opens a fresh Synchron Player in its Precision view. A Flow preset you load yourself is left alone, but Flow presets assign their controllers differently, so the dynamics may not land where the app expects.
- **Allow access to your sample drive** when macOS asks (files on a removable volume), or instruments play silence. You can change it later in System Settings → Privacy & Security.
- **Note your Synchron Player version.** Everything so far was worked out from Synchron Player AU 1.1.0; another version may store things differently.

### Loading a preset so the app sees it

- **Close Synchron's window after loading a preset.** Synchron doesn't tell the host when it loads one, so the app reads its state once a second while the window is open, and once when it closes.
- **Some values are only read when a preset loads:** the starting articulation, the Dimension Control values (Marcato's attack, XF tremolo) and Attack, Release and Legato blur. They change whenever a keyswitch or controller plays, so reading them later would make the result drift. If you change them in Synchron, load the preset again.
- **Check the right-hand panel.** The instrument profile is guessed from the preset's name and the preset folder from its folder (VelXF sus - MOD, VelXF - MOD, Velocity); presets in other folders get Velocity or VelXF sus from how the preset picks its dynamic layers. A preset whose name matches no built-in profile keeps the track's profile, often Generic, which has no playable range: keyswitches in your MIDI file that the preset's tree doesn't list are then played as notes. Pick the right profile, or make one (**File → Show Instrument Profiles Folder**; `vsl-finesse --dump-profile violins` gives a starting point).
- **Note names differ by an octave.** Synchron Player calls middle C "C4", Logic and VSL Finesse "C3": Synchron's C1 is the app's C0.

### Saving instrument details

1. Open the track's instrument button and choose **Save instrument details to Desktop**. It writes `VSL Finesse details <plug-in> <date and time>.json`: the plug-in's parameters, its programs and its saved state. It holds the track's name and the preset's settings, nothing else of yours.
2. The state is what Synchron shows at that moment, including the articulation that's selected (whatever was last picked or played). To capture a particular selection, click it in Synchron's window first. Saving one file per selection and comparing them is how the articulation tree was found: two files that differed only in the selection.
3. Read it with `scripts/synchron-state.py <file>`: the preset, its controls and the whole articulation tree as VSL Finesse reads it, with keys in Logic's note names. `--json` prints the whole state. It needs Python 3 and Zstandard (`pip install zstandard`, or Python 3.14+).

```
Articulation tree:
  (root)  [Articulation: keyswitch from C0, selected 1.0]
    C0  Short notes  [Type: keyswitch from C1 (white keys), selected 1.0]
      C1  Staccato  <01 VI staccato>  (follows velocity)
      D1  Detache  <03 VI detache>  (follows velocity)
    C#0  Long notes  [Type: keyswitch from C1, selected 2.0]
      C1  Sustain  <04 VI sustain>
      C#1  Marcato  [Attack: Dim.Ctrl/A (velocity), parallel, selected 0.97058]
        Sustain  <04 VI sustain>  (Mix)
        Staccato  <01 VI staccato>  (follows velocity)
```

Compare it with Synchron's window (remember the octave): the names, the keys, and what each Dim.Ctrl is assigned to.

### What to check

- The right-hand panel follows the preset you load.
- The **Articulation** menu matches Synchron's tree, keys included. Picking one for a passage switches Synchron (watch its window while it plays) and switches back after the passage.
- The **Report** lists what was shaped (crossfaded articulations, Attack, Release, Legato blur).
- Original against Finessed: does it sound played? Where doesn't it?
- In another DAW: the exported file opens with keyswitches before their notes and the controllers in place, and the DAW's own middle-C setting doesn't confuse the keys.

Never tried on a Mac with Synchron Player yet, so any report helps: whether Synchron honours the switch to Precision, preset following, the macOS permission prompts' text, the articulation picker, per-articulation dynamics, crossfaded articulations, and Attack, Release and Legato blur.

### What to send

Open an issue with: the instrument details file, the preset's path, your Synchron Player version, macOS version and DAW, a short MIDI file that shows the problem, the Finessed MIDI the app exported from it (before and after, if you changed something), and what you heard against what you expected. The maintainer may not have your library, so this is what your report or pull request will be judged on (see CONTRIBUTING, [Evidence the maintainer can't produce](../CONTRIBUTING.md#evidence-the-maintainer-cant-produce)). A preset whose tree looks different from the ones in [Synchron Player's saved state](#synchron-players-saved-state) becomes a test fixture.

## Command line tool

`vsl-finesse` runs the engine without the app, which is handy for batch work and for checking engine changes:

```bash
vsl-finesse Violins.mid --instrument violins --type velxf
vsl-finesse Flute.mid -p my-flute.json -s settings.json -o out.mid
vsl-finesse --list-instruments
vsl-finesse --dump-profile violins > my-violins.json
```

```
  -o, --output <file.mid>     output file (default: "<input> (Finessed).mid")
  -i, --instrument <id>       built-in instrument profile (default: generic)
  -p, --profile <file.json>   instrument profile file (overrides --instrument)
  -s, --settings <file.json>  settings file
  -t, --type <type>           Synchron preset type: velxf-sus (default), velxf or velocity
      --intensity <0..2>      overall amount of shaping
      --seed <n>              humanization seed
      --list-instruments      show built-in profiles
      --dump-profile <id>     print a built-in profile as JSON (a starting point for your own)
      --dump-settings         print the default settings as JSON
  -q, --quiet                 no report
```

It has no Synchron Player, so it doesn't know a preset's articulation tree: no per-articulation layers, crossfaded articulations or performance controls.

## Project layout

```
engine/   the performance engine: plain C++20, no dependencies
io/       MIDI files, JSON, Synchron Player's state (JUCE, Zstandard)
app/      the Mac app: tracks, plug-in hosting, playback, UI (JUCE)
cli/      vsl-finesse command line tool
tests/    unit tests
profiles/ built-in instrument profiles
scripts/  build, sign and notarize; read Synchron's saved state
docs/     methodology and this document
```

## Architecture

### Engine (`engine/`)

Plain C++ with no dependencies, so it can be built and tested anywhere. `vslf::finesse()` (`engine/include/vslf/Engine.h`) takes a `Part`, an `InstrumentProfile`, `Settings`, dynamics `Marking`s and `ArticulationPicks`, and returns the finessed part with a `Report`. Stages (`engine/src`), in order:

1. `Ingest.cpp`: separate keyswitches, remove or keep the file's CCs (`Settings::keepControllers`), resolve chords and overlaps.
2. `Articulations.cpp`: rewrite keyswitches for picked articulations (nothing happens without picks). Then `Engine.cpp` assigns each note the articulation from the latest key on each keyswitch level.
3. `Analysis.cpp`: seconds via the tempo map, metric weight from the time signature, phrase boundaries and peaks.
4. `Pauses.cpp`: breaths, string crossings and phrase lifts.
5. `Dynamics.cpp`: per-note level, velocity, swell and taper.
6. `Timing.cpp`: humanized onsets, legato overlaps, repeated-note gaps, the pauses, and the keyswitch fix-up (every keyswitch stays ahead of its note).
7. `Controllers.cpp`: the CC1/CC11 curves, replacing the file's lines for those controllers unless `Settings::replaceFileDynamics` is off.
8. `Blends.cpp`: crossfaded articulations (`Articulation::Blend`).
9. `Performance.cpp`: Attack, Release and Legato blur (`InstrumentProfile::PerformControl`).

`engine/src/Internal.h` holds the per-note analysis (`NoteInfo`) and the pipeline's shared `Context`, including the rules for whether CC1 or velocity carries a note's dynamic.

**Determinism:** all randomness is `detail::noise (seed, a, b)`, hashed from the seed and the note index, so playback, export and re-export match. Don't use another source of randomness.

### I/O (`io/`)

- `MidiIO`: reading and writing MIDI files. Events at the same tick keep a fixed order (note-offs, controllers, other channel events, keyswitches, notes), so a keyswitch at the same tick as its note still comes first.
- `JsonIO`: profiles, settings, markings and picks.
- `SynchronState`: Synchron Player's state (below).
- `BuiltinProfiles`: the JSON files in `profiles/`, compiled in.

### App (`app/Source`)

- `Session` / `Track`: tracks are independent (MIDI, profile, settings, markings, picks, result, plug-in, and what was read from Synchron). The `audio` block of each track is shared with the audio thread and only swapped under `Session::audioLock`. `Session::process` runs the engine with `Track::engineProfile()`: the instrument profile plus what the loaded preset says.
- `AppContext`: owns everything with application lifetime and implements the user actions (loading files and plug-ins, following the Synchron preset, markings and picks, settings, sessions).
- `AudioEngine`: renders every track through its plug-in and mixes them. It chases keyswitches and controllers when playback starts mid-part or A/B switches, sends note-offs on stop, and is the plug-ins' play head.
- `PlaybackSequence`: an immutable, time-sorted event list built from a part.
- UI: `MainComponent` (window, menus, Settings), `TrackListView`, `DynamicsBar` (articulation picker and dynamics buttons), `PianoRollView`, `InspectorPanel`, `WelcomeOverlay`.
- Colours: all of them are in `Theme.h` (the "wine" palette; comments name each palette token). JUCE's own widgets (buttons, menus, sliders, tooltips, alerts) get theirs from `theme::colourScheme()`, which `Main.cpp` installs as the default look and feel, so a widget only sets a colour of its own when it should differ from the rest (the Welcome screen's button, solo). Articulation colours are cycled by index and the Inspector's swatches use the same ones.
- Multitrack is already in the data model and audio engine; the UI shows one row per track.

## Instrument profiles

Built-in profiles live in [`profiles/`](../profiles). Users' edits are saved as JSON in `~/Library/Application Support/VSL Finesse/Profiles` (**File → Show Instrument Profiles Folder**). Note names use Logic's convention (C3 = middle C, MIDI 60).

```json
{
  "id": "violins",
  "name": "Violins",
  "family": "strings",
  "range": ["G2", "G6"],
  "keys": [ { "note": "C0", "level": 0 }, { "note": "C#1", "level": 1 } ],
  "articulations": [
    { "keys": ["C#0", "C#1"], "name": "Long notes - Marcato" }
  ]
}
```

- `range`: the playable range. Notes outside it are keyswitches.
- `keys`: keyswitch pitches and their level of the articulation tree. Keys not listed get their octave as their level (one octave per level, as in most Synchron presets).
- `articulations`: names for key combinations.
- Also: `dynamicsCC`, `expressionCC`, `preservedControllers` (always kept from the file), `strings` (open strings, for bow crossings), `maxBreathSeconds` (winds and brass).

With a Synchron preset loaded, the preset's own tree replaces the profile's keys and names for the keys it uses (`io::withTree`).

## Synchron Player's saved state

Reverse-engineered from "Save instrument details" dumps (Synchron Player AU 1.1.0, November 2026): Trumpet I (C) VelXF, Violin I VelXF sus, Violin I PLUS Velocity, Double basses 6 VelXF, Timpani VelXF rolls, Harp I. All of them are fixtures in `tests/SynchronTrees.h`.

**Container.** The AU state is a binary plist. Its `PluginState` entry is a Zstandard frame (`28 b5 2f fd`) holding plain JSON: `{"version": 0, "data": {...}}`. (The `data` entry next to it is the AU parameter dump.) JUCE has no Zstandard, so the build compiles the zstd library (BSD licence) from source.

**Preset and view.**
- `data.currentPreset` is the preset path, e.g. `Studio Special Editions/01 Solo strings/02 VelXF - MOD/01 Violin I - VelXF`. Flow presets start with `$FLOW_SE/`. Harp I has no preset folder in its path.
- `data.meta.gui.currentSkin` is the view: `"SynchronPlayer"` for Precision, `"Flow"` for Flow. A Flow preset also carries `data.custom.sampler.flow` and `data.meta.gui.flow`; in Precision both are absent or null.

**The articulation tree** (Synchron's Dimension Tree) is `data.custom.sampler.rootNode`. A node with children (`nodes`) is a level (a dimension); a node without plays a patch (`patchName`, `patchEntry`; the empty Custom slots have an empty `patchName`).
- `title` names the node, `subTitle` its level (Articulation, Type, Attack, XF tremolo).
- `switchControl` says what picks the child: 0 nothing; 1-128 a keyswitch (MIDI note + 1, the first child's key); then the 128 controllers; from 257 the Dimension Controls (257 = `nodeXFade0` = Dim.Ctrl/A, 258 = Dim.Ctrl/B...).
- `whiteNoteSwitch`: the children use white keys only (the violins' and basses' Short notes: Staccato C1, Detache D1).
- `currentNodeIndex`: the selected child. On a parallel level it's a fraction; in every dump it was the control's value + 0.5 (two slots), but the app doesn't rely on that.
- `parallel: true`: the children play together, mixed by the Dimension Control. Each child's `parameters.nodeXFadeMode` is 1 for Mix (always plays) and 0 for XFade (faded in by the control).
- Each patch's `parameters.velXFadeMode` = 1: the patch follows velocity even when the preset crossfades its layers (VelXF sus Staccato and Detache, timpani hits).

**Controls.**
- `data.custom.paramControl.mappings` maps each control to its source, e.g. `velXFade: {source: "cc1"}`, `nodeXFade0: {source: "velocity"}`, `release: {source: "cc23"}`. The mappings differ between Flow and Precision presets of the same instrument, which is why the app is tuned for Precision.
- `data.parameters` holds the current values (0-1): `velXFadeEnabled` (1: the layers crossfade with `velXFade`'s source; 0: velocity picks them), `nodeXFade0`..`9` (Dim.Ctrl/A...), `attack`, `release`, `legatoBlur`, and the rest of the performance and mixer parameters.

What the presets seen show:

| | Keys (Logic) | Levels | Dim.Ctrl/A | Dim.Ctrl/B | velXFadeEnabled |
|---|---|---|---|---|---|
| Trumpet I (C) VelXF | C0 / C1 | Articulation, Type | velocity (Marcato) | CC20 | 1 |
| Violin I VelXF sus | C0 / C1 | Articulation, Type (white keys under Short notes) | velocity | CC20 (XF tremolo) | 1 |
| Violin I PLUS Velocity | C0 / C1 | Articulation, Type, 31 articulations | CC1 | CC20 | 0 |
| Double basses 6 VelXF | C5 / C6 (above the range) | Articulation, Type | velocity | CC20 | 1 |
| Timpani VelXF rolls | C0 | Articulation | CC3 | CC20 | 1 |
| Harp I | A-1 | Articulation | CC1 | CC20 | 0 |

No preset seen has a third key-switched level; the levels below Type are parallel (Attack, XF tremolo). All of them map Attack to CC22 (value 0), Release to CC23 (0.5) and Legato blur to CC33 (0.235; 0 on harp and timpani).

## How the app uses Synchron's state

All of it is read by `io/SynchronState.cpp`; nothing is per instrument.

- **Opening in Precision.** Synchron Player starts in Flow. When the app loads it with no saved state (a fresh instrument, not a session), it reads the new instance's state and, if the view is Flow with no Flow preset loaded, changes `currentSkin` to `"SynchronPlayer"` and gives the state back. Only that one value is changed, as text, so the rest of the JSON stays exactly as Synchron wrote it; the other plist entries are copied byte for byte. A loaded Flow preset and a restored session are never touched.
- **Following the loaded preset.** Synchron doesn't reliably tell the host when it loads a preset, so the app reads the state once a second for tracks whose Synchron window is open (presets are only picked there) and once when the window closes. Each track remembers the last preset path it saw (`Track::synchronPreset`); only a new path changes the profile and preset folder, so a profile picked by hand isn't overruled until another preset is loaded. The preset's last part names the instrument (`01 Violin I - VelXF` → "Violin I", matched with `guessProfile`, longest match wins) and the numbered folder above it the preset folder. A preset outside the three folders gets Velocity if `velXFadeEnabled` is 0, VelXF sus otherwise.
- **The tree** (`synchronArticulations`) becomes a list of choices: a path ("Long notes > Marcato"), one key per key-switched level, the dynamic layers (`Articulation::Layers`, from `velXFadeEnabled` and the patches' `velXFadeMode`) and the crossfaded dimension it ends in, if any (`Articulation::Blend`: added layer if any slot is Mix, crossfade if all are XFade; the control's source from the mappings; sources other than velocity or a CC are skipped). It's re-read on every poll and the track re-processed only if it changed.
- **Levels from the tree.** `io::withTree` gives each of the tree's keys its depth in the tree as its keyswitch level (from `treeLevelBase`, 100, so they never meet the octave levels of other keys), because one octave per level breaks as soon as a level has more than 12 keys or doesn't start on a C. A key at two depths takes the first.
- **Values captured at preset load.** The start articulation (`currentNodeIndex` along the key-switched levels), the Dimension Control values and the performance controls are only taken when the preset changes, and saved in sessions (`Track::articulationStartKeys`, `dimensionControls`, `performControls`): they move with every keyswitch and CC the app plays, so reading them during playback would make the result drift.
- **Picks are names.** `io::resolvePicks` looks each pick's keys up again by name in the loaded preset whenever the part is processed. A name the preset doesn't have is left out, shown in the warning colour and listed in the Report.

## macOS permissions

Plug-ins run inside the app, so macOS attributes their file, network and keyboard access to VSL Finesse. The app's own code (and JUCE, as configured) uses no network, Bonjour or global keyboard APIs; the local network and Input Monitoring prompts come from the hosted plug-in. The app explains them in a first-run overlay (`WelcomeOverlay`, shown again when `welcomeVersion` in `MainComponent.cpp` is bumped) and in Info.plist purpose strings (`PLIST_TO_MERGE` in `app/CMakeLists.txt`; Input Monitoring has no purpose string).

## Versions and the changelog

- Every change that affects users (new feature, enhancement, bug fix, behaviour change, removal) gets an entry in [CHANGELOG.md](../CHANGELOG.md) in the same commit, under the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) headings (Added, Changed, Deprecated, Removed, Fixed, Security). Entries are written for users: what changed for them.
- Versions follow [semantic versioning](https://semver.org). The app is in alpha, so versions stay below 1.0.0: the **minor** version goes up for new features or behaviour changes, the **patch** version for fixes only.
- **Releasing** (pushing to `main` for users to try): move the `[Unreleased]` entries under a new version heading with the date, bump `VERSION` in the top-level `CMakeLists.txt` to match, and update the links at the bottom of the changelog.
- Pure refactors, test-only and CI-only changes don't need an entry unless they change what users get.
- Keep the documentation in step: [README.md](../README.md) for using the app, [METHODOLOGY.md](METHODOLOGY.md) for how it performs, this file for the code, [CONTRIBUTING.md](../CONTRIBUTING.md) for principles and the roadmap. Some facts are also in the app's own text (the welcome screen, Settings, tooltips) or repeated on purpose; the [agent instructions](../CLAUDE.md#where-each-change-goes) (`CLAUDE.md`, also `AGENTS.md`) have a table of every place each kind of change touches, for people and agents alike.
- These rules are also in [CLAUDE.md](../CLAUDE.md), and so are the ones under [Continuous integration](#continuous-integration): change both.

## Continuous integration

[`.github/workflows/build.yml`](../.github/workflows/build.yml) runs on pull requests (and by hand from the Actions tab), not on plain pushes or merges to `main`. Every push to a branch with an open pull request starts a build; a newer push cancels a running one. Two jobs:
- **Engine tests (Linux):** builds the engine, command line tool and tests, and runs the tests.
- **macOS app:** builds a universal app (arm64 + x86_64), runs the tests, then signs and notarizes it if the secrets are set (or ad-hoc signs it if not), and uploads **VSL-Finesse-macOS**.

The repository is public, so GitHub's standard runners, macOS included, are free ([GitHub: GitHub Actions billing](https://docs.github.com/en/billing/concepts/product-billing/github-actions)). A macOS build still takes about seven minutes and shares the repository's runners with every other pull request, so build and test locally first, batch related changes into one push, and don't push just to re-trigger a build.

**Documentation and the build.** The workflow ignores `*.md`, `docs/**`, `LICENSE` and `.gitignore` (`paths-ignore`), but for pull requests GitHub compares the whole pull request with the commit it branched from, not just the latest push ([GitHub: Workflow syntax, `paths-ignore`](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#onpushpull_requestpull_request_targetpathspaths-ignore)). So a pull request that only changes documentation never builds, but once a pull request contains code, every push to it builds both jobs, macOS included, even one that only changes a doc. Hold documentation fixes until the pull request's next code push, or open a separate documentation pull request.

## Signed builds

GitHub can sign every build with a Developer ID certificate and have Apple notarize it, so it opens with a double-click on any Mac. It needs a paid Apple Developer account, and five repository secrets. Until all of them are set, builds are unsigned (right-click > **Open** the first time).

1. **Developer ID Application certificate + private key, as a `.p12` file** (only the Account Holder can create the certificate). Either route works:
   - **In Xcode:** Settings > Accounts > your team > **Manage Certificates...** > **+** > **Developer ID Application**.
   - **On the website:** in Keychain Access, choose Keychain Access > Certificate Assistant > **Request a Certificate From a Certificate Authority...**, enter your email and save the request to disk. Upload it at developer.apple.com > Certificates > **+** > **Developer ID Application**, download the `.cer` file and double-click it.

   A `.cer` file is only the certificate; signing also needs the private key that was created on your Mac with the request. Check they're together with `security find-identity -v -p codesigning`: a line with "Developer ID Application: *your name* (*team ID*)" means they are. Then in Keychain Access (login keychain, **My Certificates**), right-click that certificate > **Export...** > **Personal Information Exchange (.p12)**, and choose a password. If the line is missing, the key isn't on this Mac: create a new request and certificate as above, and revoke the unusable one on the website.
2. **App Store Connect API key** (for notarization): on [App Store Connect](https://appstoreconnect.apple.com), go to Users and Access > Integrations > App Store Connect API > **Team Keys** and generate a key with **Developer** access. Download the `.p8` file (you can only download it once) and note its **Key ID** and the **Issuer ID** shown above the list.
3. **Add the secrets** to the repository: on GitHub, Settings > Secrets and variables > Actions > **New repository secret**. Or from Terminal in the project folder:

   ```bash
   base64 -i DeveloperID.p12 | gh secret set MACOS_CERTIFICATE_P12_BASE64
   gh secret set MACOS_CERTIFICATE_PASSWORD          # paste the .p12 password when asked
   base64 -i AuthKey_XXXXXXXXXX.p8 | gh secret set NOTARY_API_KEY_P8_BASE64
   gh secret set NOTARY_API_KEY_ID                   # e.g. XXXXXXXXXX
   gh secret set NOTARY_API_ISSUER_ID                # the Issuer ID
   ```

The next build signs the app with the hardened runtime ([`scripts/sign-and-notarize.sh`](../scripts/sign-and-notarize.sh)), sends it to Apple for notarization (usually a few minutes), and staples the ticket to the app. Then delete the `.p12` and `.p8` files from your Downloads, or keep them somewhere safe. The app's entitlements ([`app/VSLFinesse.entitlements`](../app/VSLFinesse.entitlements)) allow it to load plug-ins made by other companies, which hosts need under the hardened runtime.
