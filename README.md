# VSL Performer

A Mac app that takes a flat MIDI part (one instrument per file) and writes a natural-sounding performance for Vienna Symphonic Library's Studio Special Editions: CC1 dynamics, CC11 expression, velocities and note timing. You can audition the result through Synchron Player inside the app, compare it with the original (A/B), and export a "Finessed" MIDI file to drag back into Logic.

It never adds or changes articulation keyswitches. Keyswitches in your file are kept exactly where they are, and the app shapes the performance to suit whichever articulation is active.

## Getting the app

**Download a build.** Every push builds the app on GitHub Actions. Open the repository's **Actions** tab, click the latest successful **Build** run, and download **VSL-Performer-macOS** at the bottom. Unzip it and move **VSL Performer.app** to Applications. The build isn't notarized, so the first time you open it, right-click the app and choose **Open**.

**Or build it yourself** (needs the Xcode Command Line Tools and CMake):

```bash
xcode-select --install     # once
brew install cmake         # once
./scripts/build-mac.sh --open
```

## Using it

1. **Drop a MIDI file** onto the window: one part, one instrument. Dropping several files makes one track per file.
2. **Pick the instrument profile** (right panel). The app guesses it from the file name ("Violins.mid" → Violins). The profile's range decides which notes are keyswitches: anything outside the playable range.
3. **Choose instrument...** on the track and pick Synchron Player (Vienna instruments are listed first). Only the instrument you pick is loaded. **Edit** opens its window, where you load your Special Edition preset.
4. **Pick the Synchron preset folder** your preset came from (right panel): **VelXF sus - MOD**, **VelXF - MOD** or **Velocity**. That one choice decides how dynamics are written:
   - **VelXF sus - MOD:** long notes crossfade with CC1, short notes follow velocity
   - **VelXF - MOD:** every note crossfades with CC1. Velocities are left exactly as written, because in these presets velocity can pick an attack (e.g. Marcato's sustain/staccato attack)
   - **Velocity:** velocity carries the dynamics, and no CC1 is written

   The app remembers your last choice for new tracks. Notes written connected always overlap slightly, which triggers legato transitions in legato patches.
5. **Play** (space bar). **Original / Finessed** (or the T key) switches between the untouched file and the performance, at the same position.
6. **Shape it with buttons:**
   - **Dynamics** (the strip above the notes): drag across some bars to select them, then click **pp p mp mf f ff**, **cresc.** or **dim.** With nothing selected, a button applies to the whole part. Hover a button to see what it means. A crescendo or diminuendo grows two dynamic steps and stays there until the next marking. Right-click a marking to remove it, or use **Clear**. Esc or a click drops the selection.
   - **Character** (right panel): **Plain**, **Expressive** or **Very expressive**.
   - **Human pauses** (on by default): breaths in long wind and brass phrases, a moment for the bow to cross strings, and a tiny lift where a phrase ends without a rest. Pauses come out of the end of the earlier note, so the beat never moves.
   - **New variation** re-rolls the small human variation. **Fine-tune...** opens the individual rules behind the buttons, if you ever want them.
7. **Export Finessed MIDI...** writes `Name (Finessed).mid` next to the original. Drag it into Logic.

**File → Save Session** stores the tracks, settings, dynamics markings, and each plug-in with its loaded preset.

### What the engine does

| Step | What happens |
|---|---|
| Clean up | Keyswitches are set aside untouched. Existing CCs are removed. Chords keep their top note, and overlapping notes are trimmed, since a part should be one line. |
| Phrases | Rests, long notes and (for winds/brass) breath length split the line into phrases. Each phrase gets a peak, chosen from its high and long notes. |
| Dynamics | A phrase arch (grow to the peak, relax after), higher notes slightly louder, long notes slightly fuller. This becomes one smooth CC1 line through the whole part. |
| Velocity | Where velocity is the dynamic (Velocity presets, short notes in VelXF sus): follows the dynamic line, plus small accents on strong beats and a little humanization. Otherwise left as written. |
| Long notes | A gentle swell inside long notes (CC1, or CC11 in Velocity presets), and the last note of a phrase tapers (CC1 and CC11). |
| Timing | Small humanized timing. Connected notes overlap slightly, repeated notes get a clean gap, and wind/brass phrases get a breath. Keyswitches always stay ahead of the note they switch. |

The same file, settings and seed always produce the same result.

## Command line

The engine also runs from Terminal, which is handy for batch work:

```bash
vslp-finesse Violins.mid --instrument violins --type velxf
vslp-finesse Flute.mid -p my-flute.json -s settings.json -o out.mid
vslp-finesse --list-instruments
vslp-finesse --dump-profile violins > my-violins.json
```

## Instrument profiles

Built-in profiles live in [`profiles/`](profiles). Your edits are saved as JSON in `~/Library/Application Support/VSL Performer/Profiles` (**File → Show Instrument Profiles Folder**). Note names use Logic's convention (C3 = middle C).

```json
{
  "id": "violins",
  "name": "Violins",
  "family": "strings",
  "range": ["G2", "G6"],
  "keys": [ { "note": "C1", "level": 0 }, { "note": "C#2", "level": 1 } ],
  "articulations": [
    { "keys": ["C1", "C#2"], "name": "Long notes - Marcato" }
  ]
}
```

Synchron presets switch articulations with one key per level of the articulation tree: by default each octave is one level (C1-B1 picks the articulation, C2-B2 the type), and the current articulation is the combination of the latest key on each level. `level` overrides that for a key.

## Project layout

```
engine/   the performance engine: plain C++, no dependencies
io/       MIDI files and JSON (JUCE)
app/      the Mac app: tracks, plug-in hosting, playback, UI (JUCE)
cli/      vslp-finesse command line tool
tests/    unit tests
profiles/ built-in instrument profiles
docs/     design notes
```

Built with [JUCE](https://juce.com) 9 (AGPLv3, or a commercial licence if you ever distribute the app closed-source).
