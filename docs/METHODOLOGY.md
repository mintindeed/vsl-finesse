# How VSL Finesse performs a part

This explains how VSL Finesse turns a written part into a performance: which ideas it's built on, what it does to each note and why, and how that reaches Synchron Player. For using the app, see the [README](../README.md); for the code, [DEVELOPMENT.md](DEVELOPMENT.md).

Note names follow Logic's convention (C3 = middle C, MIDI 60) throughout.

## Contents

- [The approach](#the-approach)
- [The KTH performance rules](#the-kth-performance-rules)
- [Human touches](#human-touches)
- [Dynamics and Synchron's presets](#dynamics-and-synchrons-presets)
- [Articulations](#articulations)
- [Crossfaded articulations](#crossfaded-articulations)
- [How notes start, end and join](#how-notes-start-end-and-join)
- [Controllers already in the file](#controllers-already-in-the-file)
- [Step by step](#step-by-step)

## The approach

- **The whole part at once.** A plug-in in Logic's MIDI FX slot only sees each note as it plays: no note lengths, no next note, no phrase shape. VSL Finesse reads the whole part first, finds its phrases and their high points, and then performs it. Its output is ordinary, editable MIDI.
- **The composer chooses, the player performs.** Which instrument, preset and articulation to use, and where the dynamics markings go, are the composer's decisions: they come from your file or from you. How hard each note is played, how long it rings, how much bite it gets and where to breathe are the player's, and that's what the app does. It never picks an articulation for you.
- **It reads the instrument.** Every Synchron preset is different (its articulations, which keys switch them, how its dynamics work). The app reads all of this from the preset you've loaded instead of keeping its own tables, so it works with presets it has never seen.
- **One line per part.** One instrument per file: notes sounding at the same time are treated as mistakes. Chords keep their top note and overlapping notes are trimmed; small, deliberate overlaps between connected notes are added back later.
- **The beat never moves.** Pauses (breaths, bow crossings, phrase lifts) come out of the end of the earlier note, so the part stays in time with the rest of your song.
- **Repeatable.** All variation comes from a seed, so the same file and settings always give the same result. **New variation** changes the seed.

## The KTH performance rules

The engine is built on the **KTH rule system for music performance**, developed since the 1980s at KTH, the Royal Institute of Technology in Stockholm (Johan Sundberg, Anders Friberg and colleagues), and tested in listening experiments. Each rule is a small, explainable observation about what human players do with a written note, with one strength setting; a handful of them together makes a mechanical performance sound played. These are the rules VSL Finesse uses, adapted for Synchron Player:

| Rule | What players do | How the app applies it |
|---|---|---|
| **Phrase arch** | Grow towards the high point of a phrase and relax after it | Each phrase gets a peak (from its high and long notes); the dynamic rises to it and falls away more quickly after |
| **High loud** | Play higher notes a little louder | Small dynamic lift for notes above the phrase's average pitch |
| **Duration contrast** | Give long notes more weight than short ones | Long notes slightly fuller, short ones slightly lighter |
| **Punctuation** | Leave tiny gaps where a melody groups | A brief lift where a phrase ends without a written rest (part of Human pauses) |
| **Repetition articulation** | Separate repeated notes | Repeated pitches get a short gap so each note speaks |
| **Beat stress** | Lean slightly on strong beats | Small velocity accents on downbeats (where velocity is the dynamic) |
| **Final ending** | Ease off at the end | The last note of each phrase tapers, with the strongest taper on the final note of the piece |
| **Noise** | No two notes are exactly alike | Small random variation in timing and velocity |

The **Character** buttons set how strongly everything is applied: **Simple**, **Expressive** (the default) or **Very expressive**. They scale the rules above and everything described below.

**Phrases** are found from rests (at least 0.3 s or three quarters of a beat), long notes followed by shorter ones, the breath limit for winds and brass, and a general limit of 12 seconds. Each phrase's peak is chosen from its pitch, length, position and metre.

## Human touches

These go beyond the KTH rules, for the things orchestral players physically have to do. The first three are the **Human pauses** switch.

- **Breaths (winds and brass):** a phrase longer than the instrument's breath (about 6-8 seconds, depending on the instrument) is split at the most musical place: after a long note, at a leap, or before a strong beat. A breath (150 ms) is taken out of the end of the note before it.
- **Bow crossings (strings):** the app works out which string each note is most likely played on, keeping to low positions and avoiding needless string changes. When the bow has to jump over a string between connected notes (a cello going from the C string straight to the A string), the earlier note is shortened slightly to give it time.
- **Phrase lifts (everyone):** where a phrase ends without a written rest, the last note is shortened slightly, the way any player separates phrases.
- **Small timing and velocity variation:** note starts move by a few milliseconds and velocities by a few steps.
- **Connected notes overlap slightly** (40 ms), which triggers legato transitions in legato patches; Synchron's mono patches simply hand over to the next note. Repeated pitches never overlap; they get a small gap instead.

## Dynamics and Synchron's presets

The dynamic of each note starts from your markings (pp-ff, cresc., dim.) or the file's velocities, and is shaped by the phrase arch, high loud and duration contrast into **one smooth CC1 line** through the whole part. How that reaches the sound depends on how the preset picks its dynamic layers, which differs between Synchron's three preset folders:

| Preset folder | CC1 | Velocity | Swells inside long notes |
|---|---|---|---|
| **VelXF sus - MOD** | crossfades the layers of long notes | the dynamic for short notes; kept as written for long ones | CC1 |
| **VelXF - MOD** | crossfades the layers of every note | kept as written (except where it drives a [crossfaded articulation](#crossfaded-articulations)) | CC1 |
| **Velocity** | not written | the dynamic for every note | CC11 |

"Short" means shorter than 300 ms. But the preset knows better than that rule, so with a Synchron preset loaded the app asks it, articulation by articulation: in VelXF sus, Staccato and Detache follow velocity at any length and Sustain crossfades even when short; on the timpani (VelXF rolls), hits follow velocity and rolls crossfade. The **Velocity** folder always means velocity for every note: that's your choice. Presets outside the three folders (Harp, the timpani's VelXF rolls) are given the closest one: Velocity if the preset lets velocity pick its layers, VelXF sus otherwise.

Where velocity is the dynamic, it follows the CC1 line plus beat stress and a little variation. Otherwise it's left as written, because in these presets velocity can do something else (such as Marcato's attack).

- **Long notes swell** gently (CC1, or CC11 in Velocity presets), and the **last note of a phrase tapers** with CC1 and CC11.
- **CC11 only dips.** It rests at 127 and fades phrase endings below the softest dynamic layer (and, in Velocity presets, shapes long notes), so overall loudness stays where the mixer expects it.
- **Controllers arrive before their note** (15 ms), because the player reads them when the note starts.

## Articulations

The articulation always comes from you: from the keyswitches in your file, or from the **Articulation** picker.

**Keyswitches in your file** are kept where they are. Notes outside the instrument's playable range (or listed as keys in its profile) are keyswitches. The only change the app makes is moving one a few milliseconds earlier when timing variation would otherwise put its note first.

**Synchron's articulation tree.** A Special Edition preset switches articulations with one key per level of its tree: one key picks the articulation (Staccato, Long notes, Legato...), another the type (Sustain, Marcato...). The current articulation is the combination of the latest key on each level. The app reads the whole tree from the loaded preset, so it knows every articulation's name, its keys and its level, whatever the instrument:
- Keys aren't always where you'd expect: the Studio Special Edition trumpet and violins switch with C0 and C1, the double basses with C5 and C6 (above their range), the harp with A-1.
- Some levels use white keys only (the violins' Short notes: Staccato is C1, Detache D1).
- **Note names:** Synchron calls middle C "C4" by default, Logic "C3". So the key Synchron shows as C1 is C0 in Logic and in VSL Finesse; it's the same MIDI note (24).

**Picking an articulation** for a passage:
- Its keys are written just before the first note of the passage, only for the levels that change (Staccato after Marcato writes just the Staccato key).
- Your file's keyswitches inside the passage are replaced; the rest are kept.
- After the passage, the part switches back to what was playing before: your file's own articulation, or the one the preset was in when you loaded it. If neither is known, the pick carries on to the end.
- With picks, the preset's starting articulation is set at the first note, so playback from the top doesn't depend on what Synchron last played.
- A pick is remembered by **name**. Each preset has its own keys (Violin I PLUS adds articulations, so the same name can sit on other keys), so the keys are looked up again in whichever preset is loaded. A name the preset doesn't have writes nothing, rather than keys that now mean something else.

## Crossfaded articulations

Some articulations blend two sounds with a controller. Synchron calls the articulation tree the **Dimension Tree** and each column a **dimension**; a dimension in parallel mode plays its slots together, driven by one of eight **Dimension Controls** (Dim.Ctrl/A-H). Each slot is either **Mix** (always plays) or **XFade** (faded in by the control).

Choosing Marcato or XF tremolo is the composer's decision. How much of the blend each note gets is the player's, so the app shapes it. It tells the two kinds apart from the preset itself, without a list of articulations:

- **An added layer** (one slot Mix, the others XFade): something is added on top of a sound that always plays. Marcato, Trem. marcato and Harm. marcato add a staccato attack to the sustain. This is an **accent**, one value per note, set just before it starts:
  - more bite on strong beats, at phrase starts and after rests, on upward leaps (a fourth or more), in louder passages, and on notes your file plays louder than their neighbours (accents from a notation export);
  - less in fast notes and on repeated pitches;
  - centred on the preset's own setting (60 in the Special Editions), with a little variation, and kept away from the extremes.
- **A crossfade** (every slot XFade): one sound turns into another. XF tremolo and Sfz/Fp XF tremolo turn the note into tremolo. This is **colour**, a smooth line that leans into the second sound as a phrase builds to its peak and eases back as it relaxes, a little more in louder passages. It starts from the preset's setting (0: no tremolo), so tremolo creeps in towards climaxes. Short phrases barely move.

The value goes wherever the preset sends the control: the note's velocity in the VelXF presets, CC1 in the Velocity presets, CC20 for the tremolo crossfades. It's never written where it would fight something else: over the app's own dynamics, over velocity where velocity is the dynamic, or over a controller you kept from your file.

For Expressive, an added layer is the preset's value + 0.14 × (metric weight − 0.4), + 0.10 at a phrase start or after a gap of 0.25 s, + 0.06 on an upward leap, − up to 0.14 when the previous note started less than 0.22 s before, − 0.04 for a repeated pitch, + 0.35 × (dynamic level − 0.6), + the file's velocity above the median of its six neighbours (× 1.2, between −0.12 and +0.2), + a little variation, kept within 0.08-0.92. A crossfade is the preset's value + 0.4 × the phrase arch (0 at the start, 1 at the peak, 0 at the end; smaller for phrases under 4 s) + 0.25 × (dynamic level − 0.6) + a small drift per phrase. (Values on a 0-1 scale; × 127 for MIDI.)

## How notes start, end and join

Synchron Player's **Attack** (CC22), **Release** (CC23) and **Legato blur** (CC33) shape how each note starts, ends and joins the next. A player varies these without thinking, so the app shapes them together, note by note, around the preset's own settings:

- **Release:** notes ring into the room at phrase ends and before rests (the longer the rest, the longer the ring), most of all on the final note. Short detached notes stop cleanly, and so does a note before a breath.
- **Attack:** quiet entries (at a phrase start or after a rest, below mf, and not short) come in softly, "out of nothing": a pp entry gets about a quarter of the range. Every other note keeps the preset's attack.
- **Legato blur:** only where notes actually join. Smoother joins in slow, soft legato; cleaner ones in fast runs.

For Expressive: Release is the preset's value + 0.30 for the last note, + 0.12 to 0.24 at a phrase end or before a rest of 0.3 s or more, − up to 0.18 for notes under 0.25 s with a gap after, − 0.08 before a breath. Attack is the preset's value + 0.9 × (0.5 − dynamic level) for a quiet entry. Legato blur is the preset's value + up to 0.12 when the notes are more than 0.6 s apart, − up to 0.12 under 0.25 s, + 0.25 × (0.6 − dynamic level).

They're only written for Synchron presets that have them (another instrument's CC22 could be anything), and never over a controller that's already carrying something. Synchron's own humanize controls are left alone, since the app varies timing and velocity itself, and so are timbre, filter and dynamic range: those are tone choices, not phrasing.

## Controllers already in the file

Your file may bring controller data of its own: CC20 for a tremolo crossfade you drew, CC64 for a sustain pedal. **Settings** decides what happens to it when a file is opened: remove it **Always**, **Never**, or **Ask** when a file has some.

Kept or not, the app's own CC1 (dynamics, in crossfading presets) and CC11 (expression) replace the file's by default: one controller can't follow two lines, and those lines are the bulk of what the app does. Untick **Replace CC1 and CC11 with mine** and a kept CC1 or CC11 line stays exactly as written; the app then writes none of its own for that controller. Everything else the app writes (crossfaded articulations, attack, release, legato blur) always steps aside for a controller you kept.

## Step by step

| Step | What happens |
|---|---|
| Clean up | Keyswitches are set aside. The file's controllers are removed or kept, as you chose. Chords keep their top note and overlapping notes are trimmed. |
| Picked articulations | Keyswitches are written for the passages where you picked an articulation, and switched back after. |
| Phrases | Rests, long notes and (for winds and brass) breath length split the line into phrases, each with a peak. |
| Dynamics | Your markings or the file's velocities set the base level; phrase arch, high loud and duration contrast shape it into the CC1 line. Velocity follows it where velocity is the dynamic. Long notes swell; phrase ends taper. |
| Timing | Small timing variation, slight overlaps for connected notes, gaps for repeated notes, and the human pauses. Keyswitches always stay ahead of the note they switch. |
| Controllers | CC1 and CC11 curves, sampled every 10 ms and written only where the value changes. |
| Crossfaded articulations | Marcato's attack per note; XF tremolo along each phrase. |
| Note shapes | Attack, Release and Legato blur per note. |
