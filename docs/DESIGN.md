# Design notes

## Decisions

- **Offline, not a MIDI FX plug-in.** A plug-in in Logic's MIDI FX slot only sees each note as it plays: no note lengths, no next note, no phrase shape. An offline tool sees the whole part, and its output is ordinary, editable CC data in Logic.
- **One instrument per MIDI file.** Notes sounding at once are treated as mistakes: chords keep their top note (configurable), and overlaps are trimmed. Small, deliberate overlaps between connected notes are added back afterwards.
- **The user always chooses the articulation; the app never does.** The preset is loaded in Synchron and the articulation comes from the file's keyswitches or the picker (a selection, or the whole part). The app's job is to use what the chosen articulation offers in an organic way (its dynamics, its crossfaded dimensions), not to choose a different one, even for a plain MIDI file with no keyswitches.
- **Keyswitches are only written for picked articulations.** Notes outside the instrument's range (or listed in the profile's `keys`) are keyswitches. They're preserved; the only change is moving one a few milliseconds earlier when timing humanization would otherwise put its note first. The exception is a passage where the user picked an articulation (see below).
- **The file's CCs: Always / Never / Ask.** A setting (Settings, ⌘,) decides whether the controller data in a newly opened file is removed (Always), kept (Never), or the user is asked when the file has any (Ask, the default). The answer is per track (`Settings::keepControllers`), saved in sessions; sessions from before the setting load with removal, as they behaved. Kept or not, a controller the engine writes (CC1 in crossfading presets, CC11) replaces the file's by default: one controller can't follow two lines. A second setting, `Settings::replaceFileDynamics` (on by default, app-wide, shown under the Always/Never/Ask choice and greyed out for Always), turns that round for CC1 and CC11: with it off, a kept file's CC1 or CC11 line stays exactly as written and the engine writes none for that controller; a controller the file has no events for is still written by the app. So in Velocity presets a kept CC1 survives, which matters because it drives Marcato's attack there. The profile's `preservedControllers` are always kept.
- **The Synchron preset folder decides the strategy.** It's one choice per track:

  | Preset folder | CC1 | Velocity | Long-note swells |
  |---|---|---|---|
  | VelXF sus - MOD | drives long notes | the dynamic for short notes, kept as written for long ones | CC1 |
  | VelXF - MOD | drives every note | kept as written, except where it drives a crossfaded articulation (Marcato's attack), which the app shapes | CC1 |
  | Velocity | not written | the dynamic for every note | CC11 |

  "Short" means shorter than `shortNoteMs` (300 ms). With a Synchron preset loaded, each articulation's own layers (velocity or crossfade, read from the preset) replace the short/long rule, except in Velocity. CC1 is one continuous line through the part.
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
- `switchControl`: 0 is nothing, 1-128 a keyswitch (MIDI note + 1, the first child's key), then the 128 controllers, then the node crossfade slots from 257 (257 = `nodeXFade0`, 258 = `nodeXFade1`). What drives a slot is per preset, in `paramControl.mappings`: `nodeXFade0` is velocity in the VelXF presets (velocity picks Marcato's sustain or staccato attack), CC1 in the Velocity presets and CC3 on the timpani; `nodeXFade1` (the XF tremolo levels) is CC20. Only key-switched levels are walked: a node below which no key picks anything is one articulation (Marcato, XF tremolo).
- The trumpet: root `switchControl` 25 (MIDI 24: Staccato, 25 Long notes, 26 Legato, 27 Sforzato, 28 Custom), Long notes and Legato 37 (MIDI 36 Sustain/Legato, 37 Marcato/Legato-sus). So "Long notes > Marcato" is MIDI 25 + 37.
- **Note names.** Synchron calls middle C "C4" by default, Logic "C3" (the engine uses Logic's). So Synchron's C1 (what its screenshot shows for Staccato) is MIDI 24, Logic's C0. The earlier note here that Synchron uses C3 = middle C was wrong. One octave per level still holds (24-35 articulation, 36-47 type).

`io/SynchronState.cpp` (`synchronArticulations`) turns the tree into a list of choices (path + one key per level) and the current selection's keys.

**Nothing is per instrument.** Each Special Edition preset has its own tree: Violin I in VelXF sus - MOD, VelXF - MOD and Velocity, the PLUS versions, and every other instrument differ in their articulations, how many levels there are (Articulation, Type, sometimes a third) and which keys switch them. So the app keeps no tables of its own: every track reads the tree of the preset loaded in it, again whenever it changes, walking it to any depth. Two things follow from the tree, not from guesses:
- **Levels.** The engine's default is one octave per level, which breaks as soon as a level has more than 12 keys or doesn't start on a C. `io::withTree` gives each of the tree's keys its depth in the tree as its level (from `treeLevelBase`, 100, so they never meet the octave levels of other keys) and the tree's names for its combinations. The engine and playback use that profile (`Track::engineProfile`), so file keyswitches, switching back and "only write what changes" all follow the preset. A key that appears at two depths (not seen in VSL's presets) takes the first.
- **Picks are names.** A pick stores its name ("Long notes > Marcato") and the keys it was made with, but `io::resolvePicks` looks the keys up again by name in the loaded preset's tree each time the part is processed, because the same keys can mean something else in another preset (PLUS adds articulations; another folder can order them differently). A pick whose name isn't in the loaded preset writes nothing: it's shown in the warning colour ("not in this preset") and listed in the Report, rather than writing keys that now select another articulation. Without a tree (another instrument), picks keep their keys.

**Trees seen** (each is a test fixture in `tests/SynchronTrees.h`, cut down to the fields the reader uses): Trumpet I (C) VelXF, Violin I VelXF sus, Violin I PLUS Velocity, Double basses 6 VelXF, Timpani VelXF rolls, Harp I. What they show:
- Two key-switched levels at most (Articulation, Type); deeper levels are crossfades (Attack, XF tremolo). A third key-switched level would be read the same way, but none has been seen.
- Keys aren't always C0/C1 (Logic's naming): the double basses switch with C6/C7 (84/96), above their range; the harp with A-1 (21), one level only.
- Some levels use white keys only (`whiteNoteSwitch`: the violin's and basses' Short notes, so Detache is D1, not C#1).
- The same name has other keys in another preset: Violin I PLUS adds Detache short before Detache, so Short notes > Detache is C0 + D1 in both, but by different rules; Pizzicato is a root articulation in VelXF sus (E0) and Pizzicato + Legno > Pizzicato in PLUS.

**Dynamic layers from the preset.** The preset says how dynamics work, which is more exact than the preset folder:
- `data.parameters.velXFadeEnabled` is 1 when the layers crossfade (all the MOD presets, with `paramControl.mappings.velXFade` = CC1) and 0 when velocity picks them (Velocity presets, Harp).
- Each patch's `parameters.velXFadeMode` = 1 makes that patch follow velocity even when the preset crossfades: Staccato and Detache in VelXF sus, Single hits and Secco on the timpani. In VelXF - MOD every patch is 0.

So each articulation gets `Articulation::layers` (Velocity or Crossfade; a crossfade between patches, like Marcato, crossfades if any of them does), and the engine uses it instead of the note-length rule unless the track's preset type is Velocity (the user's choice wins). Notes before the first keyswitch get the start articulation's layers. Presets outside the three folders (Harp, "VelXF rolls - MOD") get their preset type from `velXFadeEnabled`: Velocity if 0, else VelXF sus.

In the Velocity presets, Marcato's attack (`nodeXFade0`) follows CC1, and the app writes no CC1 there, so today the attack sits wherever CC1 happens to be. See "Crossfaded articulations" under Next up.

**What the engine does with picks** (`engine/src/Articulations.cpp`, `ArticulationPicks`): for every note it works out the key it wants on each level. Inside a pick, the pick's keys; elsewhere, what the file's own keyswitches have selected by then, or the player's start articulation on levels the file never switched (only the levels picks touch are considered). A key is written at a note wherever what the player has received so far differs. The file's keyswitches governing picked notes are dropped and the rest kept as written. The timing fix-up then moves every keyswitch ahead of its (humanized) note as before. Consequences:
- Staccato after Marcato only writes C0: the Type level (C#1) stays, and Synchron ignores it under Staccato.
- After a pick the part switches back, so a pick only affects its passage. Without file keyswitches or a known start articulation the pick carries on to the end, since there's nothing known to switch back to.
- With picks, the start articulation is written at the first note when it isn't picked, so playback from the top doesn't depend on what Synchron last played.

**The start articulation** is Synchron's selection when the preset is loaded (`currentNodeIndex` along the key-switched levels). It's only taken when the preset changes: the selection changes whenever a keyswitch plays, so reading it during playback would make the result drift. Sessions save it (with the preset path), since a saved state holds whatever was played last.

The picker follows the dynamics buttons' model: drag across bars to select, then pick from the **Articulation** menu (nested like the tree); with no selection it applies to the whole part. A new pick replaces the part of an older one it covers. Without a Synchron tree (another instrument, or no preset yet), the menu offers the profile's named articulations and the key combinations in the file. The picked names also name those key combinations in the Articulations legend.

### Performance controls (built: `engine/src/Performance.cpp`)

Every Special Edition preset seen maps Synchron's performance controls the same way: Attack CC22 (0), Release CC23 (0.5) and Legato blur CC33 (0.235; 0 on harp and timpani, which have no legato), from `paramControl.mappings` and `data.parameters`. They're how a note starts, ends and joins the next one, which a player varies without thinking, so they work together and are shaped per note around the preset's value (captured at preset load, `Track::performControls`, saved in sessions) and scaled by Character:
- **Release:** + 0.30 for the last note, + 0.12 to 0.24 at a phrase end or before a rest of 0.3 s or more (longer rests ring longer), − up to 0.18 for notes under 0.25 s with a gap after, − 0.08 before a breath.
- **Attack:** + 0.9 × (0.5 − level) for an entry (phrase start or after 0.25 s) below level 0.5 that isn't short (pp ≈ + 0.23); the preset's value otherwise.
- **Legato blur:** only on connected notes (gap ≤ `legatoMaxGapMs`): + up to 0.12 when the notes are more than 0.6 s apart, − up to 0.12 under 0.25 s, + 0.25 × (0.6 − level).

Each value is set `ccLeadMs` before its note, never before the previous note starts. Nothing is written without a Synchron preset (another instrument's CC22 could be anything) or on a controller already carrying something: the file's kept controllers, the engine's dynamics and expression, a crossfaded articulation's control. Synchron's humanize controls are left alone (the app varies timing and velocity itself), and so are timbre, filter and dynamic range (tone choices, not phrasing).

## Next up

### Crossfaded articulations (built: `engine/src/Blends.cpp`)

**Terms.** VSL calls the articulation tree the **Dimension Tree**; each column is a **dimension**. A dimension is switched by keyswitches, or by one of the eight **Dimension Controls** (Dim.Ctrl/A-H, assignable to a CC, velocity, speed...). In parallel mode a dimension plays its slots together: each slot is **Mix** (always plays) or **XFade** (faded in by the Dimension Control). Here: **crossfaded articulations**. In the state: `parallel: true`, the slot's `parameters.nodeXFadeMode` (1 = Mix, 0 = XFade), `switchControl` 257 + slot (`nodeXFade0` = Dim.Ctrl/A...), and the control's source in `paramControl.mappings.nodeXFadeN`.

**Principle.** The app automates what a player varies without being told (how hard, how long, how much bite, when to breathe) and never what the composer decides (which technique). Every crossfaded articulation seen is only active when its articulation was chosen (by a keyswitch in the file or the picker): Marcato, XF tremolo. Choosing it is the composer's decision; how much of the blend each note gets is the player's, so the app shapes it. There's no list of articulations: the shape comes from the tree's structure.

1. **An added layer** (one slot Mix, the others XFade): the control adds something on top of a sound that always plays. Marcato, Trem. marcato, Harm. marcato (Sustain mixed, Staccato attack faded in). This is an accent: **one value per note**, set before its note-on and held, centred on the preset's own value (Dim.Ctrl/A is 60 in the Special Editions). More bite on downbeats, phrase starts, after rests, upward leaps, louder dynamics and notes the file plays louder than their neighbours (notated accents in a notation export); less in fast or repeated notes and inside a diminuendo; a little human variation. Character sets the range.
2. **A true crossfade** (all slots XFade): one sound turns into another. XF tremolo and Sfz/Fp XF tremolo (CC20); VSL's manual shows a Vibrato XF too. This is colour: **a smooth line** that follows the phrase's intensity, moving towards the second slot as the phrase builds to its peak and back as it relaxes, starting from the preset's value (Dim.Ctrl/B is 0: tremolo creeps in towards climaxes). Character sets the depth.

**Writing it** through whatever the preset maps the control to: a CC line (written ahead of the note like CC1, `ccLeadMs`), or the note's velocity (VelXF presets map Dim.Ctrl/A to velocity). Never where it would fight something else: not velocity when velocity is that note's dynamic, not a CC the engine writes for dynamics, and not a CC the user kept from the file (their line wins). Behaviour change: in the VelXF presets, Marcato velocities would no longer be kept exactly as written; they become the attack amount, with the file's own accents folded in.

**Data** (`io/SynchronState.cpp`, `Articulation::Blend`): an articulation ending in a parallel dimension switched by a Dimension Control (`switchControl` ≥ 257) gets its kind (any Mix slot: added layer; all XFade: crossfade; no XFade slot: nothing), its control's source (velocity or a CC; speed, aftertouch and other sources are skipped) and its name (the dimension's `subTitle`). The centre is the preset's value for that control (`data.parameters.nodeXFadeN`) when the preset was loaded, kept per track (`Track::dimensionControls`, saved in sessions) like the start articulation, because the live value moves with every CC the app plays. Without it: 60/127 for an added layer (VSL's default), 0 for a crossfade.

**Numbers** (Expressive; Character scales the deviation, intensity / 0.6): an added layer is centre + 0.14 × (metric weight − 0.4), + 0.10 at a phrase start or after a gap of 0.25 s, + 0.06 on an upward leap of a fourth or more, − up to 0.14 when the previous note started less than 0.22 s before, − 0.04 for a repeated pitch, + 0.35 × (dynamic level − 0.6), + the file's velocity above the median of its six neighbours (× 1.2, −0.12 to +0.2), + a little seeded noise; kept within 0.08–0.92. On a CC it's set `ccLeadMs` before the note, but never before the previous note has started. A crossfade is centre + 0.4 × the phrase arch (0 at the start, 1 at the peak, 0 at the end; scaled down for phrases under 4 s) + 0.25 × (level − 0.6) + a small drift per phrase, sampled every 2 × `ccIntervalMs` through each note. On velocity it can only be one value per note.

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
