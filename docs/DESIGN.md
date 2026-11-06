# Design notes

## Decisions

- **Offline, not a MIDI FX plug-in.** A plug-in in Logic's MIDI FX slot only sees each note as it plays: no note lengths, no next note, no phrase shape. An offline tool sees the whole part, and its output is ordinary, editable CC data in Logic.
- **One instrument per MIDI file.** Notes sounding at once are treated as mistakes: chords keep their top note (configurable), and overlaps are trimmed. Legato overlaps are added back deliberately afterwards, only for legato patches.
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

## Ideas for later

- Per-family rule tuning (string bow changes, brass attack weight).
- Time-varying base dynamic from the notation (pp/ff markers, hairpins) when the export doesn't encode them as velocity.
- Repetition patterns (alternating emphasis on repeated shorts).
- Ensemble awareness in multitrack: balance and shared phrase peaks across parts.
- Drag the finessed file straight from the app into Logic.
- Notarized builds (needs an Apple Developer account).
