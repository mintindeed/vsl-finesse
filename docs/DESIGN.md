# Design notes

## Decisions

- **Offline, not a MIDI FX plug-in.** A plug-in in Logic's MIDI FX slot only sees each note as it plays: no note lengths, no next note, no phrase shape. An offline tool sees the whole part, and its output is ordinary, editable CC data in Logic.
- **One instrument per MIDI file.** Notes sounding at once are treated as mistakes: chords keep their top note (configurable), and overlaps are trimmed. Small, deliberate overlaps between connected notes are added back afterwards.
- **Keyswitches are never written.** Notes outside the instrument's range (or listed in the profile's `keys`) are keyswitches. They're preserved; the only change is moving one a few milliseconds earlier when timing humanization would otherwise put its note first.
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
2. `Engine.cpp`: assign each note the articulation from the latest key on each keyswitch level.
3. `Analysis.cpp`: seconds via the tempo map, metric weight from the time signature, phrase boundaries (rests ≥ max(0.3 s, ¾ beat); long notes followed by shorter ones; breath limit for winds/brass; 12 s general limit), phrase peaks (pitch, length, position, metre).
4. `Dynamics.cpp`: per-note level = base (input velocity or a fixed level) + phrase arch + high-loud + duration contrast; velocity = level + metric accent + humanization; swell and taper amounts.
5. `Timing.cpp`: humanized onsets and per-articulation attack offsets; legato overlaps (not between repeated pitches, not across real rests); repeated-note gaps; breaths; keyswitch fix-up.
6. `Controllers.cpp`: CC1/CC11 curves sampled every `ccIntervalMs`, written only when the value changes.

## App (`app/Source`)

- `Session` / `Track`: tracks are independent (MIDI, profile, settings, result, plug-in). The `audio` block of each track is shared with the audio thread and only swapped under `Session::audioLock`.
- `AudioEngine`: renders every track's sequence through its plug-in and mixes them. It chases keyswitches and controllers when playback starts mid-part or A/B switches, and sends note-offs on stop. It's the plug-ins' play head (tempo and bar position from the part).
- `PlaybackSequence`: an immutable, time-sorted event list built from a part.
- Multitrack is already in the data model and audio engine; the UI shows one row per track.

## Next up

### Articulation picker (by name)

Goal: pick an articulation such as "Long notes > Marcato" by name over a selected passage (or the whole part), and have the app write the right keyswitches. Ideally the app reads the articulation tree from the Synchron Player preset that's loaded, instead of the user typing anything.

What we know from the Studio Special Edition trumpet preset (Synchron Player screenshot):
- The tree has levels. **Articulation** (C1 Staccato, C#1 Long notes, D1 Legato, D#1 Sforzato, E1 Custom), then **Type** (C2 Sustain, C#2 Marcato), then **Attack** (Sustain/Staccato), which is picked by a controller or by velocity (threshold 76 in that preset), not by a key.
- One octave per level is already the engine's default (C1 + C#2 = one articulation).
- Keyswitch names here use Synchron's/Logic's convention (C3 = middle C).

Waiting on: two files from "Save instrument details to Desktop" (instrument menu on the track), one with Long notes > Marcato selected in Synchron and one with Staccato. Compare them to find where Synchron stores the tree and the current selection (state blob, parameters or program names). If the tree is readable, parse it; if not, fall back to a picker built from the keys used in the file plus names the user types once per instrument profile.

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

The picker should follow the dynamics buttons' model: drag across bars to select, then choose; with no selection it applies to the whole part. Keyswitches it writes must still land just before the notes they switch.

### Smarter breaths

Today winds and brass breathe after a fixed amount of playing (`maxBreathSeconds` per profile, about 6-8 s). Make breath capacity depend on what's being played: loud, high and long-held passages use air faster (especially high brass), soft and low ones slower; a full breath at a real rest refills it, a short catch-breath only partly. Breathe at the most musical place within the allowed window (after long notes, before strong beats, at leaps). Pauses stay inside the earlier note so the beat never moves.

## Ideas for later

- Per-family rule tuning (string bow changes, brass attack weight).
- Time-varying base dynamic from the notation (pp/ff markers, hairpins) when the export doesn't encode them as velocity.
- Repetition patterns (alternating emphasis on repeated shorts).
- Ensemble awareness in multitrack: balance and shared phrase peaks across parts.
- Drag the finessed file straight from the app into Logic.
- Notarized builds (needs an Apple Developer account).
