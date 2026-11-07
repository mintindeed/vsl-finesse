# VSL Finesse

A Mac app that takes a flat MIDI part (one instrument per file) and writes a natural-sounding performance for Vienna Symphonic Library's Studio Special Editions: CC1 dynamics, CC11 expression, velocities and note timing. You can audition the result through Synchron Player inside the app, compare it with the original (A/B), and export a "Finessed" MIDI file to drag back into Logic.

It never adds or changes articulation keyswitches. Keyswitches in your file are kept exactly where they are, and the app shapes the performance to suit whichever articulation is active.

## Getting the app

**Download a build.** Every push builds the app on GitHub Actions. Open the repository's **Actions** tab, click the latest successful **Build** run, and download **VSL-Finesse-macOS** at the bottom. Unzip it and move **VSL Finesse.app** to Applications. Builds are signed and notarized by Apple (see [Signed builds](#signed-builds)), so it opens with a double-click.

**Or build it yourself** (needs the Xcode Command Line Tools and CMake):

```bash
xcode-select --install     # once
brew install cmake         # once
./scripts/build-mac.sh --open
```

## Using it

1. **Drop a MIDI file** onto the window: one part, one instrument. Dropping several files makes one track per file.
2. **Pick the instrument profile** (right panel). The app guesses it from the file name ("Violins.mid" → Violins). The profile's range decides which notes are keyswitches: anything outside the playable range.
3. **Synchron Player** loads on the track by itself when you open a MIDI file, if it's installed. **Edit** opens its window, where you load your Special Edition preset. To use another instrument, click the instrument button on the track (Vienna instruments are listed first). Only the instrument you pick is loaded.
4. **Pick the Synchron preset folder** your preset came from (right panel): **VelXF sus - MOD**, **VelXF - MOD** or **Velocity**. That one choice decides how dynamics are written:
   - **VelXF sus - MOD:** long notes crossfade with CC1, short notes follow velocity
   - **VelXF - MOD:** every note crossfades with CC1. Velocities are left exactly as written, because in these presets velocity can pick an attack (e.g. Marcato's sustain/staccato attack)
   - **Velocity:** velocity carries the dynamics, and no CC1 is written

   The app remembers your last choice for new tracks. Notes written connected always overlap slightly, which triggers legato transitions in legato patches.
5. **Play** (space bar). **Original / Finessed** (or the T key) switches between the untouched file and the performance, at the same position.
6. **Shape it with buttons:**
   - **Dynamics** (the strip above the notes): drag across some bars to select them, then click **pp p mp mf f ff**, **cresc.** or **dim.** With nothing selected, a button applies to the whole part. Hover a button to see what it means. A crescendo or diminuendo grows two dynamic steps and stays there until the next marking. Right-click a marking to remove it, or use **Clear**. Esc or a click drops the selection.
   - **Character** (right panel): **Simple**, **Expressive** or **Very expressive**.
   - **Human pauses** (on by default): breaths in long wind and brass phrases, a moment for the bow to cross strings, and a tiny lift where a phrase ends without a rest. Pauses come out of the end of the earlier note, so the beat never moves.
   - **New variation** re-rolls the small human variation. **Fine-tune...** opens the individual rules behind the buttons, if you ever want them.
7. **Export Finessed MIDI...** writes `Name (Finessed).mid` next to the original. Drag it into Logic.

**File → Save Session** stores the tracks, settings, dynamics markings, and each plug-in with its loaded preset.

## How it performs

### The KTH performance rules

The engine is built on the ideas of the **KTH rule system for music performance**, developed since the 1980s at KTH, the Royal Institute of Technology in Stockholm (Johan Sundberg, Anders Friberg and colleagues), and tested in listening experiments. Each rule is a small, explainable observation about what human players do with a written note. Each rule has one strength setting, and combining a handful of rules makes a mechanical performance sound played. The rules below are the ones VSL Finesse uses, adapted for Synchron Player's CC1, CC11 and velocity.

| Rule | What players do | How the app applies it |
|---|---|---|
| **Phrase arch** | Grow towards the high point of a phrase and relax after it | Each phrase gets a peak (from its high and long notes); the dynamic rises to it and falls away more quickly after |
| **High loud** | Play higher notes a little louder | Small dynamic lift for notes above the phrase's average pitch |
| **Duration contrast** | Give long notes more weight than short ones | Long notes slightly fuller, short ones slightly lighter |
| **Punctuation** | Leave tiny gaps where a melody groups | A brief lift where a phrase ends without a written rest (part of Human pauses) |
| **Repetition articulation** | Separate repeated notes | Repeated pitches get a short gap so each note speaks |
| **Beat stress** | Lean slightly on strong beats | Small velocity accents on downbeats (where velocity is the dynamic) |
| **Final ending** | Ease off at the end | The last note of each phrase tapers, with the strongest taper on the final note of the piece |
| **Noise** | No two notes are exactly alike | Small random variation in timing and velocity (see below) |

The **Character** buttons set how strongly the rules are applied together: **Simple**, **Expressive** (the default) or **Very expressive**.

### Human touches

These go beyond the KTH rules, for the things orchestral players physically have to do:

- **Breaths (winds and brass):** a phrase longer than the instrument's breath (about 6-8 seconds, depending on the instrument) is split at the most musical place: after a long note, at a leap, or before a strong beat. A breath (150 ms) is taken out of the end of the note before it.
- **Bow crossings (strings):** the app works out which string each note is most likely played on, keeping to low positions and avoiding needless string changes. When the bow has to jump over a string between connected notes (a cello going from the C string straight to the A string), the earlier note is shortened slightly to give it time.
- **Phrase lifts (everyone):** where a phrase ends without a written rest, the last note is shortened slightly, the way any player separates phrases.
- **Small timing and velocity variation:** note starts move by a few milliseconds and velocities by a few steps. It's repeatable: the same file and settings always give the same result. **New variation** gives a different, equally repeatable version.
- **Connected notes overlap slightly**, which triggers legato transitions in legato patches.

The breaths, bow crossings and phrase lifts are the **Human pauses** switch. Pauses always come out of the end of the earlier note, so the beat never moves and the part stays in time with the rest of your song.

### What the engine does, step by step

| Step | What happens |
|---|---|
| Clean up | Keyswitches are set aside untouched. Existing CCs are removed. Chords keep their top note, and overlapping notes are trimmed, since a part should be one line. |
| Phrases | Rests, long notes and (for winds/brass) breath length split the line into phrases. Each phrase gets a peak, chosen from its high and long notes. |
| Dynamics | Your markings (pp-ff, cresc., dim.) or the file's velocities set the base level. Phrase arch, high loud and duration contrast shape it into one smooth CC1 line through the whole part. |
| Velocity | Where velocity is the dynamic (Velocity presets, short notes in VelXF sus): follows the dynamic line, plus beat stress and a little variation. Otherwise left as written. |
| Long notes | A gentle swell inside long notes (CC1, or CC11 in Velocity presets), and the last note of a phrase tapers (CC1 and CC11). |
| Timing | Small timing variation, slight overlaps for connected notes, gaps for repeated notes, and the human pauses. Keyswitches always stay ahead of the note they switch. |

## Command line

The engine also runs from Terminal, which is handy for batch work:

```bash
vsl-finesse Violins.mid --instrument violins --type velxf
vsl-finesse Flute.mid -p my-flute.json -s settings.json -o out.mid
vsl-finesse --list-instruments
vsl-finesse --dump-profile violins > my-violins.json
```

## Instrument profiles

Built-in profiles live in [`profiles/`](profiles). Your edits are saved as JSON in `~/Library/Application Support/VSL Finesse/Profiles` (**File → Show Instrument Profiles Folder**). Note names use Logic's convention (C3 = middle C).

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

The next build signs the app with the hardened runtime, sends it to Apple for notarization (usually a few minutes), and staples the ticket to the app. Then delete the `.p12` and `.p8` files from your Downloads, or keep them somewhere safe. The app's entitlements ([app/VSLFinesse.entitlements](app/VSLFinesse.entitlements)) allow it to load plug-ins made by other companies, which hosts need under the hardened runtime.

## Changes

See [CHANGELOG.md](CHANGELOG.md). Versions follow [semantic versioning](https://semver.org); the app is in alpha, so versions stay below 1.0.0 for now.

## Project layout

```
engine/   the performance engine: plain C++, no dependencies
io/       MIDI files and JSON (JUCE)
app/      the Mac app: tracks, plug-in hosting, playback, UI (JUCE)
cli/      vsl-finesse command line tool
tests/    unit tests
profiles/ built-in instrument profiles
docs/     design notes
```

## Licence

VSL Finesse's own code is released under the [MIT licence](LICENSE).

The app and the command line tool are built with [JUCE](https://juce.com) 9, which is licensed separately: AGPLv3, or a JUCE commercial licence. A build you give to other people that links JUCE under the AGPLv3 has to be passed on under the AGPLv3 as a whole, with its source. The engine (`engine/`) doesn't use JUCE and is plain MIT.
