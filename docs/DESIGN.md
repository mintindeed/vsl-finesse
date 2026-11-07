# Design notes

## Decisions

- **Offline, not a MIDI FX plug-in.** A plug-in in Logic's MIDI FX slot only sees each note as it plays: no note lengths, no next note, no phrase shape. An offline tool sees the whole part, and its output is ordinary, editable CC data in Logic.
- **One instrument per MIDI file.** Notes sounding at once are treated as mistakes: chords keep their top note (configurable), and overlaps are trimmed. Small, deliberate overlaps between connected notes are added back afterwards.
- **Keyswitches are only written for picked articulations.** Notes outside the instrument's range (or listed in the profile's `keys`) are keyswitches. They're preserved; the only change is moving one a few milliseconds earlier when timing humanization would otherwise put its note first. The exception is a passage where the user picked an articulation (see below).
- **Existing CCs are wiped**, except controllers listed in the profile's `preservedControllers`.
- **The Synchron preset folder decides the strategy.** It's one choice per track:

  | Preset folder | CC1 | Velocity | Long-note swells |
  |---|---|---|---|
  | VelXF sus - MOD | drives long notes | the dynamic for short notes, kept as written for long ones | CC1 |
  | VelXF - MOD | drives every note | kept as written (it can select an attack variant) | CC1 |
  | Velocity | not written | the dynamic for every note | CC11 |

  "Short" means shorter than `shortNoteMs` (300 ms). CC1 is one continuous line through the part.
- **Connected notes always overlap slightly** (`legatoOverlapMs`): legato patches play their transitions, and Synchron's mono patches hand over to the next note. Repeated pitches never overlap.
- **CC11 only dips.** It rests at `expressionBase` (127) and is used to fade phrase endings below the softest dynamic layer (and, in Velocity presets, to shape long notes). Overall loudness stays where the mixer expects it.
- **CC values arrive before their note** (`ccLeadMs`, 15 ms), because the player reads CC1 at note-on.
- **Deterministic.** All randomness is hashed from the seed and the note index, so playback, export and re-export match.

## Engine pipeline (`engine/src`)

1. `Ingest.cpp`: separate keyswitches, wipe CCs, resolve chords and overlaps.
2. `Articulations.cpp`: rewrite keyswitches for picked articulations (nothing happens without picks). Then `Engine.cpp`: assign each note the articulation from the latest key on each keyswitch level.
3. `Analysis.cpp`: seconds via the tempo map, metric weight from the time signature, phrase boundaries (rests ≥ max(0.3 s, ¾ beat); long notes followed by shorter ones; breath limit for winds/brass; 12 s general limit), phrase peaks (pitch, length, position, metre).
4. `Dynamics.cpp`: per-note level = base (input velocity or a fixed level) + phrase arch + high-loud + duration contrast; velocity = level + metric accent + humanization; swell and taper amounts.
5. `Timing.cpp`: humanized onsets and per-articulation attack offsets; legato overlaps (not between repeated pitches, not across real rests); repeated-note gaps; breaths; keyswitch fix-up.
6. `Controllers.cpp`: CC1/CC11 curves sampled every `ccIntervalMs`, written only when the value changes.

## App (`app/Source`)

- `Session` / `Track`: tracks are independent (MIDI, profile, settings, result, plug-in). The `audio` block of each track is shared with the audio thread and only swapped under `Session::audioLock`.
- `AudioEngine`: renders every track's sequence through its plug-in and mixes them. It chases keyswitches and controllers when playback starts mid-part or A/B switches, and sends note-offs on stop. It's the plug-ins' play head (tempo and bar position from the part).
- `PlaybackSequence`: an immutable, time-sorted event list built from a part.
- Multitrack is already in the data model and audio engine; the UI shows one row per track.

## Synchron Player

**Synchron Player's state is readable** (found from two "Save instrument details" dumps, Synchron Player AU 1.1.0, November 2026):
- The AU state is a binary plist. Its `PluginState` entry is a Zstandard frame (`28 b5 2f fd`) holding plain JSON: `{"version": 0, "data": {...}}`. (The `data` entry next to it is the AU parameter dump.)
- `data.currentPreset` is the preset path, e.g. `Studio Special Editions/01 Solo strings/02 VelXF - MOD/01 Violin I - VelXF`. Flow presets start with `$FLOW_SE/`.
- `data.meta.gui.currentSkin` is the view: `"SynchronPlayer"` for Precision, `"Flow"` for Flow. A Flow preset also carries `data.custom.sampler.flow` and `data.meta.gui.flow`; in Precision both are absent or null.
- The articulation tree is `data.custom.sampler.rootNode`: `nodes` (each with a `title`, e.g. Short notes, Long notes, Legato, and its own children) and `currentNodeIndex` for the current selection.
- CC assignments are in `data.custom.paramControl.mappings` (e.g. `nodeXFade0: {source: "cc3"}`). They differ between Flow and Precision presets of the same instrument, so the engine's CC1/velocity strategy only holds for the Precision presets.

JUCE has no Zstandard, so the app builds the zstd library (BSD licence) from source. `io/SynchronState.cpp` reads the plist and the JSON.

**Opening in Precision.** Synchron Player starts in Flow, but the engine's dynamics strategies are written for the Precision presets. When the app loads Synchron Player with no saved state (a fresh instrument, not a session), it reads the new instance's state and, if the view is Flow with no Flow preset loaded, changes `currentSkin` to `"SynchronPlayer"` and gives the state back. Only that one value is changed, as text, so the rest of the JSON stays exactly as Synchron wrote it; the other plist entries are copied byte for byte. A loaded Flow preset and a restored session are never touched.

**Permissions.** Plug-ins run inside the app, so macOS attributes their file, network and keyboard access to VSL Finesse. The app's own code (and JUCE, as configured) uses no network, Bonjour or global keyboard APIs; the local network and Input Monitoring prompts come from the hosted plug-in. The app explains them in a first-run overlay (`WelcomeOverlay`, shown again when `welcomeVersion` is bumped) and in Info.plist purpose strings (`PLIST_TO_MERGE` in `app/CMakeLists.txt`; Input Monitoring has no purpose string).

**Following the loaded preset.** The instrument profile and preset folder follow `currentPreset`. Its last part names the instrument (`01 Violin I - VelXF` → "Violin I", matched to a profile with `guessProfile`, longest match wins), and the numbered folder above it is the preset folder (`02 VelXF - MOD`). Synchron doesn't reliably tell the host when it loads a preset, so the app reads the state once a second for tracks whose Synchron window is open (presets are only picked there) and once when the window closes. Each track remembers the last preset path it saw; only a new path changes anything, so a profile picked by hand isn't overruled until another preset is loaded. The preset a plug-in starts with (none, or the session's) counts as seen.

### Articulation picker

Pick an articulation such as "Long notes > Marcato" by name over a selected passage (or the whole part), and the app writes its keyswitches. The names and keys come from the Synchron preset that's loaded.

**Where Synchron keeps the tree** (two "Save instrument details" dumps of the Studio Special Edition Trumpet I (C), VelXF - MOD, one with Long notes > Marcato selected and one with Staccato; the decompressed states differ only in the root's `currentNodeIndex`, 1 vs 0):
- `data.custom.sampler.rootNode` is the tree. A node with children (`nodes`) is a level: `subTitle` names it (Articulation, Type, Attack), `switchControl` says what picks the child, `whiteNoteSwitch` makes the children use white keys only, and `currentNodeIndex` is the selected child (a fraction on a crossfaded level, e.g. 0.97 for the Attack, which follows velocity). A node without children plays a patch (`patchName`, `patchEntry`); the Custom slots have an empty `patchName` and are skipped.
- `switchControl`: 0 is nothing, 1-128 a keyswitch (MIDI note + 1, the first child's key), then the 128 controllers, then velocity (257, matching `paramControl.mappings.nodeXFade0: velocity`). Only key-switched levels are walked: a node below which no key picks anything is one articulation (Marcato, whose Attack level follows velocity).
- The trumpet: root `switchControl` 25 (MIDI 24: Staccato, 25 Long notes, 26 Legato, 27 Sforzato, 28 Custom), Long notes and Legato 37 (MIDI 36 Sustain/Legato, 37 Marcato/Legato-sus). So "Long notes > Marcato" is MIDI 25 + 37.
- **Note names.** Synchron calls middle C "C4" by default, Logic "C3" (the engine uses Logic's). So Synchron's C1 (what its screenshot shows for Staccato) is MIDI 24, Logic's C0. The earlier note here that Synchron uses C3 = middle C was wrong. One octave per level still holds (24-35 articulation, 36-47 type).

`io/SynchronState.cpp` (`synchronArticulations`) turns the tree into a list of choices (path + one key per level) and the current selection's keys.

**What the engine does with picks** (`engine/src/Articulations.cpp`, `ArticulationPicks`): for every note it works out the key it wants on each level. Inside a pick, the pick's keys; elsewhere, what the file's own keyswitches have selected by then, or the player's start articulation on levels the file never switched (only the levels picks touch are considered). A key is written at a note wherever what the player has received so far differs. The file's keyswitches governing picked notes are dropped and the rest kept as written. The timing fix-up then moves every keyswitch ahead of its (humanized) note as before. Consequences:
- Staccato after Marcato only writes C0: the Type level (C#1) stays, and Synchron ignores it under Staccato.
- After a pick the part switches back, so a pick only affects its passage. Without file keyswitches or a known start articulation the pick carries on to the end, since there's nothing known to switch back to.
- With picks, the start articulation is written at the first note when it isn't picked, so playback from the top doesn't depend on what Synchron last played.

**The start articulation** is Synchron's selection when the preset is loaded (`currentNodeIndex` along the key-switched levels). It's only taken when the preset changes: the selection changes whenever a keyswitch plays, so reading it during playback would make the result drift. Sessions save it (with the preset path), since a saved state holds whatever was played last.

The picker follows the dynamics buttons' model: drag across bars to select, then pick from the **Articulation** menu; with no selection it applies to the whole part. A new pick replaces the part of an older one it covers. Without a Synchron tree (another instrument, or no preset yet), the menu offers the profile's named articulations and the key combinations in the file. The picked names also name those key combinations in the Articulations legend.

## Next up

### Smarter breaths

Today winds and brass breathe after a fixed amount of playing (`maxBreathSeconds` per profile, about 6-8 s). Make breath capacity depend on what's being played: loud, high and long-held passages use air faster (especially high brass), soft and low ones slower; a full breath at a real rest refills it, a short catch-breath only partly. Breathe at the most musical place within the allowed window (after long notes, before strong beats, at leaps). Pauses stay inside the earlier note so the beat never moves.

### Onboarding as permission checks

Replace the one-off welcome overlay with a series of checks that run whenever something the app needs is missing: at launch, and again when the app comes back to the front (the user may have changed System Settings meanwhile). Each failed check shows its own full-window screen; when all pass, nothing is shown.

1. **Is Vienna Synchron Player installed?** If not: a screen saying the app was designed for VSL Synchron Player and may not work well with other virtual instruments. This one is informational, so it can be dismissed and is remembered until Synchron Player is installed (or removed again).
2. **macOS permissions.** For each permission: check it, and if it's missing show step-by-step instructions (what it's for, what to click in the macOS prompt, or where it is in System Settings → Privacy & Security if it was denied) with a button that opens the right Settings pane. The screen only lets you continue once the permission is granted.

**Order matters.** macOS asks the moment protected access happens, whoever does it. The checks must finish before anything that can trigger a prompt:
- No plug-in is loaded (Synchron Player reads samples from removable volumes and talks to the network as soon as it loads): no default instrument, no session restore.
- Files passed in at launch (opened from Finder, dropped on the Dock icon) are queued and opened afterwards.
- No file dialog opens in a protected folder.

The main window itself can be built behind the onboarding screens, since building it touches nothing protected, but the steps above are held back until onboarding finishes. A launch gate in `Main.cpp` that creates `AppContext`'s plug-in work only after the checks pass keeps this in one place.

**What macOS lets an app check (to confirm when building this):**

| Permission | Can it be checked without triggering the prompt? | Does the app itself need it? |
| --- | --- | --- |
| Input Monitoring ("keystrokes from any app") | Yes: `CGPreflightListenEventAccess()`; `CGRequestListenEventAccess()` asks. | No. A plug-in asks for it. |
| Files on removable volumes | No public API. The only test is to touch a file there, which itself shows the prompt. | Only if the Vienna library is on an external drive (Synchron Player needs it). |
| Files in Documents (and Desktop, Downloads) | Same: no check without access. | Only to save sessions there (the default save location). |
| Local network | No public API to check. | No. Vienna's software asks for it. |

Consequences to decide before building:
- **Gating on permissions the app doesn't need is wrong.** Input Monitoring and local network are asked for by the plug-in, not needed by the app. A screen that won't let you continue until they're granted would force users to grant access the app doesn't use. Those two should be explained, not required.
- **File permissions can't be checked silently.** "Check, then explain" isn't possible for them: the check is the prompt. Instead, explain first, then deliberately trigger the prompt from the onboarding screen (e.g. list the folder holding the Vienna library, found from Vienna's settings or picked by the user), and read the result. If it was denied, show the System Settings steps and re-test on return.
- macOS shows each prompt only once. After a denial, only System Settings can change it, so the steps must cover that path. The Settings deep links (`x-apple.systempreferences:com.apple.preference.security?Privacy_…`) change between macOS versions and need checking on current macOS.
- The purpose strings in Info.plist stay: they're what macOS shows inside its own prompt.

## Ideas for later

- Per-family rule tuning (string bow changes, brass attack weight).
- Time-varying base dynamic from the notation (pp/ff markers, hairpins) when the export doesn't encode them as velocity.
- Repetition patterns (alternating emphasis on repeated shorts).
- Ensemble awareness in multitrack: balance and shared phrase peaks across parts.
- Drag the finessed file straight from the app into Logic.
- Notarized builds (needs an Apple Developer account).
