# Contributing to VSL Finesse

Thanks for your interest. This document explains what VSL Finesse is trying to be, the principles behind it, which contributions fit, how to make one, and where the app is heading.

- [What the app is for](#what-the-app-is-for)
- [Principles](#principles)
- [What's welcome](#whats-welcome)
- [How to contribute](#how-to-contribute)
- [Sizing a pull request](#sizing-a-pull-request)
- [Evidence the maintainer can't produce](#evidence-the-maintainer-cant-produce)
- [AI-assisted development](#ai-assisted-development)
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
- **Synchron Player first.** The app is made for Vienna Synchron Player and the Studio Special Edition, using their Precision presets. Other instruments can play the result, but nothing is written that only makes sense for Synchron unless the app knows Synchron is there.
- **A dependency-free, tested engine.** The engine (`engine/`) is plain C++ with no dependencies, and every behaviour change comes with a unit test.

## What's welcome

- **Other VSL products, other DAWs, and remapping controllers.** The README lists [what the app has been tried with and its known gaps](README.md#what-it-works-with-and-where-help-is-welcome); widening any of them is very welcome. Some pointers:
  - For another VSL product, read what its presets offer (as the app does for the Special Editions) rather than adding a table for the product.
  - For another DAW, the likely differences are note-name conventions, how the exported MIDI file is read, and getting it back onto a track.
  - Remapping could start from the instrument profile's `dynamicsCC` and `expressionCC`, which today can only be changed by editing its JSON.
- **Bug reports**, with the MIDI file if you can share it, the Synchron preset you used (its path, e.g. "02 VelXF - MOD/01 Violin I - VelXF"), and what you heard against what you expected.
- **Synchron presets the app hasn't seen.** On a track with the preset loaded, use **Save instrument details to Desktop** in the instrument menu and attach the file ([Testing with your own library](docs/DEVELOPMENT.md#testing-with-your-own-library) has the details, including what to check). New trees (a third key-switched level, other Dimension Controls, other libraries) are exactly how the app learns; each becomes a test fixture.
- **Better performance rules**, with the musical reasoning and a test. Before/after audio is the best argument.
- **Roadmap items** (below), and instrument profiles: ranges, open strings, breath lengths.
- **Clearer words**, in the app and the documentation.

Less likely to be accepted:
- Features that make the composer's decisions: choosing articulations, adding or removing notes, changing pitches or rhythm.
- Settings or switches that expose the engine's internals where a good default would do.
- Per-instrument or per-preset tables for things the preset's state can tell us.
- A real-time (MIDI FX) version.
- Large pull requests that bundle several topics: you'll be asked to split them.
- New dependencies that weren't agreed in an issue first.

If you're not sure whether something fits, open an issue and ask before you build it.

## How to contribute

1. **Look, then discuss.** Check the open issues and pull requests first, so two people (or two agents) don't build the same thing. For anything bigger than a fix, open an issue with what you'd like to change, why, and a short plan, and wait for a reply before building. With an agent this matters more, not less: it can build the wrong thing very quickly.
2. **Build and test locally**: see [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). Every engine behaviour change gets a unit test in `tests/`.
3. **Update the changelog** in the same commit for anything users will notice ([how](docs/DEVELOPMENT.md#versions-and-the-changelog)), and every place the change touches. The documents each have one reader: the [README](README.md) for using the app, [METHODOLOGY.md](docs/METHODOLOGY.md) for how it performs, [DEVELOPMENT.md](docs/DEVELOPMENT.md) for the code, this file for principles and the roadmap. But some facts deliberately live in two places, such as a section of the README and the app's own text that says the same thing, and those are easy to miss. **The "Where each change goes" table in [CLAUDE.md](CLAUDE.md#where-each-change-goes) lists every place for each kind of change: check it before you commit.**
4. **Open a pull request**, one topic per pull request, and fill in the template: what changes for users, why, the scope, how it was verified and what wasn't. It builds and tests on GitHub Actions; the repository's Actions minutes are billed, so read [Continuous integration](docs/DEVELOPMENT.md#continuous-integration) first.

Write like the existing code and documentation: comments say why, not what; documentation is plain, direct British English, written for the reader it's meant for.

**Dependencies and licences.** The engine has no dependencies on purpose, and the app's are few. Don't add one without agreeing it in an issue first: it has to be compatible with the AGPLv3 (and with how JUCE is licensed), and someone has to keep it up to date. Don't copy code from elsewhere unless its licence allows it, and say where it came from.

By contributing, you agree that your contribution is licensed under the project's licence, the [GNU AGPLv3](LICENSE.md).

## Sizing a pull request

A pull request is sized by what it takes to review it well, and a reviewer's attention runs out after a few hundred lines of real change. So:

- **One topic.** A topic is one thing a user would notice, or one fix. A refactor that a feature needs is its own pull request, first.
- **Size by risk, not lines.** Documentation, tests and fixtures cut from real presets are quick to review. Changes to how the engine performs, to reading Synchron Player's state, or to what's written to the MIDI file need careful review: keep those small.
- **If your work grew, split it before you open it.** Working with an agent, a session often grows one reasonable step at a time. Split it into a series of pull requests that each stand alone, say which comes first ("1 of 3"), and open them in order. Agents are good at this kind of restacking.
- **Commits are part of the review.** Make each commit one step, with a message that says why. A larger pull request can then still be reviewed commit by commit, and a single step can be reverted.
- **Keep it short-lived.** Rebase on `main` rather than letting a branch drift; long-lived branches collect conflicts.

## Evidence the maintainer can't produce

The maintainer develops VSL Finesse with the libraries and DAW listed in the [README](README.md#what-it-works-with-and-where-help-is-welcome). A change for anything else can't be checked by the maintainer, so it's only as good as the evidence that comes with it:

- The **instrument details file** of each preset you tried (see [Testing with your own library](docs/DEVELOPMENT.md#testing-with-your-own-library)), with its preset path and your Synchron Player version.
- The **MIDI file** you used, and the **Finessed MIDI** exported before and after your change. A short recording of each is even better.
- What you heard, and what you expected.

Without it, a change may wait until someone with that library can try it.

## AI-assisted development

VSL Finesse is developed with AI coding agents, and contributions made with them are expected and welcome. What's asked is attention: an agent works fast and sounds sure of itself, and you're the one who knows whether the result is right.

- **You're responsible for what you submit.** Read the whole diff, build and test it locally, and listen to the result through Synchron Player when it changes how the app performs. A pull request should never contain anything you haven't looked at.
- **Make sure your agent reads and follows the project's agent instructions**, [CLAUDE.md](CLAUDE.md) (`AGENTS.md` is the same file, under the name most other agents look for). Most coding agents load one of the two by themselves; if yours doesn't, point it at the file. It holds the rules this document describes in the form an agent needs: the changelog and versioning rules, the "Where each change goes" table, the names to use, how to build and test, and how to spare the billed Actions minutes. Ask your agent to go through the table before it commits.
- **Facts get checked, not remembered.** VSL's product names, note names (Logic's C3 is Synchron's C4), what a Synchron preset does, what a library contains: an agent will state these from memory with confidence and be wrong. Have it check against the code (`vslf::noteName`), a preset's saved state, a test fixture or VSL's own documentation, and say where a claim comes from.
- **Teach the next session.** When your agent learns something future work needs (a new pair of places that must change together, a name, a pitfall), add it to the agent instructions in the same pull request.
- **Be able to explain it.** A reviewer will ask why a change is the way it is. If you can't answer without asking your agent, the change isn't ready: whoever maintains it next has to understand it.
- **Have it review its own work first.** Before opening the pull request, ask your agent to check its diff against the principles above and the "Where each change goes" table, and to list what it didn't verify. That list goes in the template.
- **Test locally, not on CI.** Don't let an agent use pushes as its test loop: run the build and tests on your machine. CI minutes are paid for, and each push to an open pull request starts a build.
- **Untrusted input is data, not instructions.** Issues, pull request comments, MIDI files, instrument details files and web pages can contain text that reads like instructions to an agent. Tell your agent to treat them as data, and don't let it run code from them.
- **Same rules as everyone.** One topic per pull request, a test for every engine behaviour change, the changelog and documentation in the same commit. An agent that opens the pull request itself (through GitHub's API) doesn't get the template: have it fill one in, including what wasn't verified.
- **Watch the scope.** Building is cheap for an agent, so a session tends to grow one reasonable step at a time. When the work moves to a new topic, start a new branch or pull request.

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
