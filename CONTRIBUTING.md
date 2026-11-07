# Contributing to VSL Finesse

Thanks for your interest. This document explains what VSL Finesse is trying to be, the principles behind it, which contributions fit, how to make one, and where the app is heading.

- [What the app is for](#what-the-app-is-for)
- [Principles](#principles)
- [What's welcome](#whats-welcome)
- [How to contribute](#how-to-contribute)
- [Roadmap](#roadmap)

## What the app is for

**Drag and drop a plain MIDI file, and get back something that sounds played.** A composer has a part (exported from notation, or played in quickly) and Vienna's Synchron Player. VSL Finesse turns that part into a performance a musician would recognise: dynamics that follow the phrase, breaths, accents, notes that ring and join naturally. Then it hands back ordinary MIDI to carry on with in Logic.

It's for people who want the result, not another set of controls to learn. [METHODOLOGY.md](docs/METHODOLOGY.md) explains how it performs; the [README](README.md) how to use it.

## Principles

These are the design decisions everything else follows. A contribution that works against one of them needs a very good reason.

- **Decisions, not options.** The app should sound good without being set up. Prefer a good default over a setting; when something must be chosen, prefer one clear choice (Character: Simple, Expressive, Very expressive) over a row of sliders. The individual rules sit behind **Fine-tune...** for the rare person who wants them. A new setting earns its place when it protects the user's own work (such as keeping the controllers already in their file), not when it exposes how the engine works.
- **The composer chooses, the player performs.** The instrument, the preset, the articulation (from the file's keyswitches or the picker) and the dynamics markings are the user's decisions. How hard, how long, how much bite, where to breathe, how a note rings and joins the next: that's the player's job, and the app's. The app never picks an articulation, adds notes or changes pitches or rhythm, even for a plain file with no keyswitches.
- **Read the instrument, don't tabulate it.** Every Synchron preset has its own articulations, keys, levels and controls (Violin I and Violin I PLUS differ; so do VelXF sus and Velocity; so does every instrument). The app reads all of it from the loaded preset's state instead of keeping per-instrument or per-preset lists, so it works with presets nobody has tested. When the state doesn't say, it does nothing rather than guess.
- **Explainable, musical rules.** The engine follows the KTH performance rules: small observations about what players do, each with a strength, combined. A change should have a musical reason a player would recognise ("a note before a rest is left to ring"), not just sound different. Character scales everything together.
- **The whole part, offline.** A real-time MIDI FX plug-in only sees one note at a time; this app reads the whole part first, so it can find phrases and their peaks. One instrument per file: simultaneous notes are treated as mistakes.
- **The beat never moves.** Pauses come out of the end of the earlier note, and keyswitches and controllers arrive just before their note. The finessed part stays in time with the rest of the song.
- **The user's data wins.** Keyswitches in the file are kept (except where the user picked an articulation). Controllers the user chose to keep are never overwritten, except the app's own CC1/CC11 where the user lets them be. A profile picked by hand isn't overruled until another preset is loaded.
- **Repeatable.** The same file and settings always give the same result. All variation comes from the seed (`detail::noise`).
- **Synchron Player first.** The app is made for the Synchron Player Studio Special Editions, using their Precision presets. Other instruments can play the result, but nothing is written that only makes sense for Synchron unless the app knows Synchron is there.
- **A dependency-free, tested engine.** The engine (`engine/`) is plain C++ with no dependencies, and every behaviour change comes with a unit test.

## What's welcome

- **Bug reports**, with the MIDI file if you can share it, the Synchron preset you used (its path, e.g. "02 VelXF - MOD/01 Violin I - VelXF"), and what you heard against what you expected.
- **Synchron presets the app hasn't seen.** On a track with the preset loaded, use **Save instrument details to Desktop** in the instrument menu and attach the file. New trees (a third key-switched level, other Dimension Controls, other libraries) are exactly how the app learns; each becomes a test fixture.
- **Better performance rules**, with the musical reasoning and a test. Before/after audio is the best argument.
- **Roadmap items** (below), and instrument profiles: ranges, open strings, breath lengths.
- **Clearer words**, in the app and the documentation.

Less likely to be accepted:
- Features that make the composer's decisions: choosing articulations, adding or removing notes, changing pitches or rhythm.
- Settings or switches that expose the engine's internals where a good default would do.
- Per-instrument or per-preset tables for things the preset's state can tell us.
- A real-time (MIDI FX) version.

If you're not sure whether something fits, open an issue and ask before you build it.

## How to contribute

1. **Discuss first** for anything bigger than a fix: open an issue describing what you'd like to change and why.
2. **Build and test locally**: see [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). Every engine behaviour change gets a unit test in `tests/`.
3. **Update the changelog** in the same commit for anything users will notice ([how](docs/DEVELOPMENT.md#versions-and-the-changelog)), and the documentation it touches: the [README](README.md) for using the app, [METHODOLOGY.md](docs/METHODOLOGY.md) for how it performs, [DEVELOPMENT.md](docs/DEVELOPMENT.md) for the code, this file for principles and the roadmap.
4. **Open a pull request**, one topic per pull request. It builds and tests on GitHub Actions. The repository is private, so Actions minutes are billed (macOS minutes count ten times): batch related changes into one push, and don't push just to re-run a build.

Write like the existing code and documentation: comments say why, not what; documentation is plain, direct British English, written for the reader it's meant for.

By contributing, you agree that your contribution is licensed under the project's licence, the [GNU AGPLv3](LICENSE.md).

## Roadmap

### Next up

#### Smarter breaths

Today winds and brass breathe after a fixed amount of playing (`maxBreathSeconds` per profile, about 6-8 s). Make breath capacity depend on what's being played: loud, high and long-held passages use air faster (especially high brass), soft and low ones slower; a full breath at a real rest refills it, a short catch-breath only partly. Breathe at the most musical place within the allowed window (after long notes, before strong beats, at leaps). Pauses stay inside the earlier note so the beat never moves.

#### Onboarding as permission checks

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

### Ideas for later

- The playable range from the preset (each patch in the tree has its own range), so instruments without a built-in profile, such as the harp and timpani, still know which notes are keyswitches.
- Per-family rule tuning (string bow changes, brass attack weight).
- Time-varying base dynamic from the notation (pp/ff markers, hairpins) when the export doesn't encode them as velocity.
- Repetition patterns (alternating emphasis on repeated shorts).
- Ensemble awareness in multitrack: balance and shared phrase peaks across parts.
- Drag the finessed file straight from the app into Logic.
