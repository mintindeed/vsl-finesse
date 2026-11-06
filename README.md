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
3. **Set each articulation's patch type.** The Articulations list shows every keyswitch (or keyswitch combination) used in the file. For each one, choose the patch type you loaded in Synchron Player:
   - **Velocity:** velocity picks the dynamic layer, CC1 does nothing (shorts, pizzicato...)
   - **VelXF sus - MOD:** velocity shapes the attack, CC1 crossfades the sustain
   - **VelXF - MOD:** CC1 crossfades the whole note

   Tick **Legato** for legato patches, so notes overlap and trigger the legato transitions. These choices are saved to your copy of the instrument profile, so you set them once per instrument.
4. **Choose instrument...** on the track loads a plug-in (e.g. Synchron Player). The first time, use **Scan for plug-ins...** in that menu. **Edit** opens the plug-in's window, where you load your Special Edition preset.
5. **Play** (space bar). **Original / Finessed** (or the T key) switches between the untouched file and the performance, at the same position.
6. Adjust the **Performance** sliders. Changes apply immediately (double-click a slider to reset it). **New variation** re-rolls the humanization.
7. **Export Finessed MIDI...** writes `Name (Finessed).mid` next to the original. Drag it into Logic.

**File → Save Session** stores the tracks, settings, and each plug-in with its loaded preset.

### What the engine does

| Step | What happens |
|---|---|
| Clean up | Keyswitches are set aside untouched. Existing CCs are removed. Chords keep their top note, and overlapping notes are trimmed, since a part should be one line. |
| Phrases | Rests, long notes and (for winds/brass) breath length split the line into phrases. Each phrase gets a peak, chosen from its high and long notes. |
| Dynamics | A phrase arch (grow to the peak, relax after), higher notes slightly louder, long notes slightly fuller. This becomes one smooth CC1 line through the whole part. |
| Velocity | Follows CC1, plus small accents on strong beats and a little humanization. This is what Velocity patches respond to. |
| Long notes | Crossfading patches get a gentle swell inside long notes, and the last note of a phrase tapers (CC1 and CC11). |
| Timing | Small humanized timing. Legato patches get overlaps, repeated notes get a clean gap, and wind/brass phrases get a breath. Keyswitches always stay ahead of the note they switch. |

The same file, settings and seed always produce the same result.

## Command line

The engine also runs from Terminal, which is handy for batch work:

```bash
vslp-finesse Violins.mid --instrument violins
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
    { "keys": ["C1", "C#2"], "name": "Legato", "mode": "velxf-sus", "legato": true }
  ]
}
```

`level` is for presets that switch articulations with one key per level of the Synchron articulation tree. The articulation is then the combination of the latest key on each level. Leave everything at level 0 for one-key-per-articulation presets.

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
