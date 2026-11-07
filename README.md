# VSL Finesse

Turn a plain MIDI part into a natural-sounding performance for Vienna Symphonic Library's Synchron Player.

Drop in a flat MIDI file (one instrument per file) and VSL Finesse plays it the way a musician would: phrases that rise and fall, breaths, accents, notes that ring at the end of a phrase and come in softly when they're quiet. You hear the result through Synchron Player inside the app, compare it with the original at the press of a key, and export a "Finessed" MIDI file to drag back into Logic.

## Contents

- [Background](#background)
- [Install](#install)
- [Usage](#usage)
- [Contributing](#contributing)
- [Licence](#licence)

## Background

MIDI from a notation program or a quickly played-in sketch sounds mechanical: every note the same loudness, the same length, the same attack. Vienna's Synchron Player can do much better, but only if it's fed the right controllers (dynamics on CC1, expression on CC11, attack, release...) in the right shape, note by note. Drawing all of that by hand takes hours per part.

VSL Finesse writes it for you. It's built around the **KTH rules for music performance**, decades of research into what players do with a written note, plus the things orchestral players physically have to do (breathe, cross strings, end a phrase). It reads the Synchron preset you've loaded, so it knows what each articulation can do and shapes it accordingly.

You stay in charge of the music: **you** choose the instrument, the preset and the articulations (Staccato, Long notes > Marcato...). The app decides how a good player would perform them. [How it performs](docs/METHODOLOGY.md) explains it all in detail.

### What it works with, and where help is welcome

VSL Finesse is developed with what its author owns:
- **Vienna Synchron Player** with the **Studio Special Edition Vol. 1 and Vol. 2** (including their PLUS articulations), using their **Precision** presets, and the free **Big Bang Orchestra** collections.
- **Logic Pro**: the app follows Logic's note names (C3 = middle C), and its result is meant to go back into Logic.

Contributions that widen this are very welcome:
- **Other VSL products.** Other Synchron libraries, the paid Big Bang Orchestra collections, the earlier SYNCHRON-ized Special Edition, Vienna Instruments: anything that tests VSL Finesse with them and makes it work well. The app reads each preset's articulations and controls rather than relying on a list, so many may already work, but nobody has tried.
- **Other DAWs.** The exported file is standard MIDI, so any DAW can open it, but only Logic has been tried. Reports and fixes for Cubase, Studio One, Digital Performer, Reaper and the rest are welcome.
- **Remapping controllers.** The app writes dynamics to CC1 and expression to CC11, as the Special Editions' Precision presets expect. It doesn't follow you if you've assigned Synchron's controls to other CCs, or translate to the controller numbers of your own template, and it does no other remapping. Requests and contributions for this are welcome too.

Other instruments can play the result, but may not behave as intended.

## Install

You need:
- A Mac.
- [Vienna Synchron Player](https://www.vsl.co.at) with the Studio Special Edition installed (Vienna Assistant installs both).
- A DAW to take the result back into, such as Logic Pro.

**Download the app.** Builds are made on GitHub for each pull request: open the pull request's **Checks** tab (or the repository's **Actions** tab, the latest successful **Build** run) and download **VSL-Finesse-macOS** at the bottom. Unzip it and move **VSL Finesse.app** to Applications. Builds are signed and notarized by Apple, so it opens with a double-click.

You can also [build it yourself](docs/DEVELOPMENT.md#building).

### Permissions macOS asks for

The first time it starts, the app shows a welcome screen explaining these (open it again from **Help → Welcome and Permissions...**). Synchron Player runs inside the app, so macOS asks about anything Synchron Player does in VSL Finesse's name:

- **Files on a removable volume:** Synchron Player reads its samples from your Vienna library, often on an external drive. Allow it, or those instruments play silence.
- **Files in your Documents folder:** new sessions are saved there by default, and Synchron Player may keep presets and settings there.
- **Find devices on your local network:** asked by Vienna's software when Synchron Player loads. VSL Finesse itself doesn't use the network. You can deny it.
- **Receive keystrokes from any app** (Input Monitoring): asked by a plug-in. VSL Finesse only reads the keyboard while its own window is in front, and doesn't need this. You can deny it.

You can change your answers in System Settings → Privacy & Security.

## Usage

### Hear your part played

1. **Drop a MIDI file** onto the window: one part, one instrument. Dropping several files makes one track per file. If the file has controller data of its own (CC20, CC64...), the app asks whether to keep it.
2. Synchron Player loads on the track by itself. Click **Edit** to open it and **load your Special Edition preset** (a Precision preset, e.g. "02 VelXF - MOD/01 Violin I - VelXF"). The app follows it: the instrument and the preset folder in the right-hand panel are set for you.
3. **Play** (space bar). **Original / Finessed** (or the T key) switches between your untouched file and the performance, at the same position.
4. **Export Finessed MIDI...** writes `Name (Finessed).mid` next to the original. Drag it into Logic, onto a track with the same Synchron preset.

That's it. Everything below is optional.

### Shape it

All of these work the same way: drag across some bars in the piano roll to select them, then click. With nothing selected, a button applies to the whole part. Esc or a click drops the selection.

- **Articulation:** pick one from the loaded preset, e.g. **Long notes > Marcato** or **Staccato**. The app writes the keyswitches for it and switches back after the passage. Your file's own keyswitches are kept everywhere else. If you load a different preset that doesn't have that articulation, the pick turns orange and does nothing until you pick again.
- **Dynamics:** **pp p mp mf f ff**, **cresc.** or **dim.** (hover a button to see what it means). A crescendo or diminuendo grows two dynamic steps and stays there until the next marking.

Picks and markings show above the notes. Right-click one to remove it, or use **Clear** (dynamics) or **Remove picked articulations** (in the Articulation menu).

In the right-hand panel:
- **Character:** **Simple**, **Expressive** (the default) or **Very expressive**: how strongly everything is shaped.
- **Human pauses** (on by default): breaths in long wind and brass phrases, a moment for the bow to cross strings, and a tiny lift where a phrase ends without a rest. They come out of the end of the earlier note, so the beat never moves.
- **New variation** gives a different version of the small human variation (each version is repeatable). **Fine-tune...** opens the individual rules, if you ever want them.
- **Synchron preset folder** (VelXF sus - MOD, VelXF - MOD, Velocity) and the **instrument profile** are normally set for you from the preset. You can change them by hand; they follow Synchron again when you load another preset.

### Settings

**Settings** (⌘,) decides what happens to controller data already in a MIDI file: remove it **Always**, **Never**, or **Ask** when a file has some (the default). If you keep it, the app's own dynamics (CC1) and expression (CC11) still replace the file's, since that's the bulk of what it does; untick **Replace CC1 and CC11 with mine** to let your file's lines win.

### Sessions

**File → Save Session** stores the tracks, settings, markings, picked articulations, and each instrument with its loaded preset.

### Command line

The engine also runs from Terminal, which is handy for batch work. See [the command line tool](docs/DEVELOPMENT.md#command-line-tool).

## Contributing

Ideas, bug reports and contributions are welcome, especially for [other VSL products, other DAWs and remapping controllers](#what-it-works-with-and-where-help-is-welcome). [CONTRIBUTING.md](CONTRIBUTING.md) explains what the app is trying to be, which contributions fit, and the roadmap. [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) covers building, testing and how the code fits together. Changes between versions are in [CHANGELOG.md](CHANGELOG.md).

## Licence

Copyright (C) 2026 Gabriel Koen.

VSL Finesse is free software under the [GNU Affero General Public License, version 3](LICENSE.md) (AGPLv3): you can use, share and change it, and anything you pass on that's built from it has to be passed on under the same licence, with its source.

It's AGPLv3 because it's built with [JUCE](https://juce.com) 9, which is offered under the AGPLv3 or a paid JUCE licence. It also uses [Zstandard](https://github.com/facebook/zstd) (BSD licence) to read Synchron Player's saved state.
