#include <doctest/doctest.h>

#include "TestHelpers.h"

#include <cmath>

using namespace vslf;
using namespace vslf::test;

TEST_CASE ("tempo map converts ticks and seconds across tempo changes")
{
    TempoMap map (480, { { 0, 500000.0 }, { 1920, 1000000.0 } }); // 120 bpm, then 60 bpm at bar 2
    CHECK (map.secondsAt (480) == doctest::Approx (0.5));
    CHECK (map.secondsAt (1920) == doctest::Approx (2.0));
    CHECK (map.secondsAt (2400) == doctest::Approx (3.0));
    CHECK (map.tickAt (3.0) == doctest::Approx (2400));
    CHECK (map.bpmAt (100) == doctest::Approx (120));
    CHECK (map.bpmAt (2000) == doctest::Approx (60));
    CHECK (map.ticksForMs (0, 500) == doctest::Approx (480));
}

TEST_CASE ("note names use Logic's convention")
{
    CHECK (noteName (60) == "C3");
    CHECK (noteName (36) == "C1");
    CHECK (noteName (49) == "C#2");
    CHECK (noteName (0) == "C-2");
    CHECK (parseNoteName ("C3") == 60);
    CHECK (parseNoteName ("C#2") == 49);
    CHECK (parseNoteName ("Db2") == 49);
    CHECK (parseNoteName ("C-2") == 0);
    CHECK (parseNoteName ("G8") == 127);
    CHECK_FALSE (parseNoteName ("H2").has_value());
    CHECK_FALSE (parseNoteName ("C").has_value());
}

TEST_CASE ("ingest separates keyswitches, wipes CCs and removes overlaps")
{
    auto profile = violinProfile();
    profile.preservedControllers = { 32 };

    auto part = makePart ({ note (0, 480, 67), note (0, 480, 71),   // chord: keep 71
                            note (480, 720, 72), note (960, 480, 74) }, // overlap: trim to 960
                          { note (0, 60, 36), note (940, 60, 49) });  // keyswitches C1, C#2
    part.controllers = { { 0, 1, 64, 1 }, { 0, 11, 100, 1 }, { 0, 32, 3, 1 } };

    Report report;
    const auto out = ingest (part, profile, Settings {}, report);

    REQUIRE (out.notes.size() == 3);
    CHECK (out.notes[0].pitch == 71);
    CHECK (out.notes[1].end == 960);
    CHECK (out.keyswitches.size() == 2);
    CHECK (out.keyswitches[0].pitch == 36);
    CHECK (out.keyswitches[1].pitch == 49);
    REQUIRE (out.controllers.size() == 1);
    CHECK (out.controllers[0].controller == 32);
    CHECK (report.chordNotesRemoved == 1);
    CHECK (report.overlapsTrimmed == 1);
    CHECK (report.controllersRemoved == 2);
}

TEST_CASE ("keyswitches survive the pipeline and stay ahead of their notes")
{
    auto profile = violinProfile();
    Settings s;
    s.humanizeTimingMs = 12.0;

    std::vector<Note> ks;
    auto notes = line (16, 240);
    for (int i = 0; i < 16; i += 4)
        ks.push_back (note (notes[(size_t) i].start, 60, i % 8 == 0 ? 36 : 49));

    const auto result = finesse (makePart (notes, ks), profile, s);
    const auto& out = result.part;

    REQUIRE (out.keyswitches.size() == ks.size());
    for (size_t k = 0; k < ks.size(); ++k)
    {
        CHECK (out.keyswitches[k].pitch == ks[k].pitch);
        // The note it governs is note 4k; the keyswitch must come first.
        const auto& governed = out.notes[k * 4];
        // (At tick 0 it can't move earlier; the writer orders keyswitches first.)
        if (governed.start > 0)
            CHECK (out.keyswitches[k].start < governed.start);
        else
            CHECK (out.keyswitches[k].start == 0);
        if (k > 0)
            CHECK (out.keyswitches[k].start > out.notes[k * 4 - 1].start);
    }
}

TEST_CASE ("connected notes overlap slightly, repeated pitches get a gap")
{
    Settings s;
    s.humanizeTimingMs = 0.0;

    std::vector<Note> notes = { note (0, 480, 67), note (480, 480, 67), note (960, 480, 69), note (1440, 480, 71) };
    const auto result = finesse (makePart (notes), violinProfile(), s);
    const auto& out = result.part.notes;
    REQUIRE (out.size() == 4);
    CHECK (out[0].end < out[1].start);  // G -> G re-articulated
    CHECK (out[1].end > out[2].start);  // G -> A connected
    CHECK (out[2].end > out[3].start);  // A -> B connected
    CHECK (out[1].end - out[2].start <= 48); // a small overlap (~40 ms), never a doubled note
    CHECK (result.report.legatoConnections == 2);
}

TEST_CASE ("legato isn't bridged across real rests")
{
    std::vector<Note> notes = { note (0, 480, 67), note (960, 480, 69) };
    const auto result = finesse (makePart (notes), violinProfile(), Settings {});
    CHECK (result.part.notes[0].end < result.part.notes[1].start);
}

TEST_CASE ("VelXF - MOD: CC1 carries the dynamics, velocities are left alone")
{
    Settings s;
    s.presetType = PresetType::VelXF;
    const auto result = finesse (makePart (line (16, 480)), violinProfile(), s);
    const auto& out = result.part;
    const auto cc1 = controllersFor (out, 1);
    REQUIRE_FALSE (cc1.empty());
    CHECK (cc1.front().tick <= out.notes.front().start);
    for (size_t i = 1; i < cc1.size(); ++i)
        CHECK (cc1[i - 1].tick <= cc1[i].tick);

    // Velocity may select an attack variant in these presets, so it isn't touched.
    for (const auto& n : out.notes)
        CHECK (n.velocity == 80);

    CHECK_FALSE (controllersFor (out, 11).empty());
}

TEST_CASE ("Velocity: velocities carry the dynamics and no CC1 is written")
{
    Settings s;
    s.presetType = PresetType::Velocity;
    const auto result = finesse (makePart (line (16, 480)), violinProfile(), s);
    CHECK (controllersFor (result.part, 1).empty());

    bool varied = false;
    for (const auto& n : result.part.notes)
        varied |= n.velocity != 80;
    CHECK (varied);
}

TEST_CASE ("VelXF sus - MOD: short notes follow velocity, long notes follow CC1")
{
    Settings s;
    s.presetType = PresetType::VelXFSustain;
    s.humanizeTimingMs = 0.0;

    // Eight staccato-length notes, then four long ones.
    std::vector<Note> notes;
    for (int i = 0; i < 8; ++i)
        notes.push_back (note (i * 240, 100, 72 + (i % 3), 80)); // ~100 ms
    for (int i = 0; i < 4; ++i)
        notes.push_back (note (1920 + i * 960, 960, 72 + i, 80)); // 1 s

    const auto result = finesse (makePart (notes), violinProfile(), s);
    const auto& out = result.part.notes;
    REQUIRE (out.size() == 12);

    bool shortsShaped = false;
    for (size_t i = 0; i < 8; ++i)
        shortsShaped |= out[i].velocity != 80;
    CHECK (shortsShaped);

    for (size_t i = 8; i < 12; ++i)
        CHECK (out[i].velocity == 80);

    CHECK_FALSE (controllersFor (result.part, 1).empty());
}

TEST_CASE ("results are deterministic for a seed")
{
    Settings a;
    a.seed = 7;
    const auto part = makePart (line (24, 240));
    const auto r1 = finesse (part, violinProfile(), a);
    const auto r2 = finesse (part, violinProfile(), a);
    REQUIRE (r1.part.notes.size() == r2.part.notes.size());
    for (size_t i = 0; i < r1.part.notes.size(); ++i)
    {
        CHECK (r1.part.notes[i].start == r2.part.notes[i].start);
        CHECK (r1.part.notes[i].velocity == r2.part.notes[i].velocity);
    }
    CHECK (r1.part.controllers.size() == r2.part.controllers.size());

    Settings b = a;
    b.seed = 8;
    const auto r3 = finesse (part, violinProfile(), b);
    bool anyDifferent = false;
    for (size_t i = 0; i < r1.part.notes.size(); ++i)
        anyDifferent |= r1.part.notes[i].start != r3.part.notes[i].start || r1.part.notes[i].velocity != r3.part.notes[i].velocity;
    CHECK (anyDifferent);
}

TEST_CASE ("rests split phrases, and a phrase arches up and back down")
{
    Settings s;
    s.humanizeVelocity = 0.0;
    s.humanizeTimingMs = 0.0;
    s.highLoudAmount = 0.0;
    s.durationContrastAmount = 0.0;

    // Two 4-second phrases of quarter notes on one pitch, a bar of rest between.
    std::vector<Note> notes;
    for (int i = 0; i < 8; ++i)
        notes.push_back (note (i * 480, 480, 72, 80));
    for (int i = 0; i < 8; ++i)
        notes.push_back (note (5760 + i * 480, 480, 72, 80));

    const auto result = finesse (makePart (notes), violinProfile(), s);
    REQUIRE (result.phrases.size() == 2);

    const auto& out = result.part;
    const int start = controllerAt (out, 1, out.notes[0].start);
    const int middle = controllerAt (out, 1, out.notes[5].start);
    const int end = controllerAt (out, 1, out.notes[7].start);
    CHECK (middle > start);
    CHECK (middle > end);
}

TEST_CASE ("long wind phrases are split for breaths")
{
    InstrumentProfile flute;
    flute.family = Family::Woodwinds;
    flute.lowestNote = 60;
    flute.highestNote = 96;
    flute.maxBreathSeconds = 8.0;

    // 20 seconds of continuous eighth notes.
    const auto result = finesse (makePart (line (80, 240, 72)), flute, Settings {});
    CHECK (result.phrases.size() >= 3);
    for (const auto& p : result.phrases)
        CHECK (p.endSeconds - p.startSeconds <= 8.5);

    // A breath leaves a gap after the phrase-final note.
    const auto& firstEnd = result.part.notes[result.phrases[0].lastNote];
    const auto& nextStart = result.part.notes[result.phrases[1].firstNote];
    // ~100 ms at 120 bpm: the breath is capped so a short note keeps 60% of its length.
    CHECK (nextStart.start - firstEnd.end >= 90);
}

TEST_CASE ("multi-level keyswitches resolve to the combination")
{
    auto profile = violinProfile();
    profile.keys = { { 36, 0 }, { 37, 0 }, { 49, 1 }, { 50, 1 } };
    profile.articulations = {
        { { 36, 49 }, { "Long legato", 0.0 } },
        { { 37, 49 }, { "Staccato", 0.0 } },
        { { 36 }, { "Long", 0.0 } },
    };

    auto notes = line (12, 480);
    std::vector<Note> ks = {
        note (0, 30, 36), note (0, 30, 49),          // C1 + C#2 -> Long legato
        note (1920, 30, 37),                         // C#1 (+ C#2 still held) -> Staccato
        note (3840, 30, 36), note (3840, 30, 50),    // C1 + D2 -> only C1 is mapped -> Long
    };

    const auto result = finesse (makePart (notes, ks), profile, Settings {});
    auto nameOf = [&result] (size_t i) { return result.articulations[(size_t) result.articulationIndex[i]].name; };
    CHECK (nameOf (0) == "Long legato");
    CHECK (nameOf (3) == "Long legato");
    CHECK (nameOf (4) == "Staccato");
    CHECK (nameOf (8) == "Long");
    CHECK (result.report.warnings.empty());
}

TEST_CASE ("keyswitch combinations without a name are listed, not warned about")
{
    auto notes = line (4, 480);
    const auto result = finesse (makePart (notes, { note (0, 30, 40) }), violinProfile(), Settings {});
    REQUIRE (result.report.articulations.size() == 1);
    CHECK_FALSE (result.report.articulations[0].mapped);
    CHECK (result.report.warnings.empty());
}

TEST_CASE ("keyswitch levels default to one octave per level")
{
    // No levels in the profile: C1 (Articulation) and C#2 (Type) combine,
    // like the Synchron Special Edition presets.
    auto notes = line (8, 480);
    std::vector<Note> ks = { note (0, 30, 36), note (0, 30, 49), note (1920, 30, 37) };
    const auto result = finesse (makePart (notes, ks), violinProfile(), Settings {});
    REQUIRE (result.articulationKeys.size() == 2);
    CHECK (result.articulationKeys[(size_t) result.articulationIndex[0]] == std::vector<int> { 36, 49 });
    CHECK (result.articulationKeys[(size_t) result.articulationIndex[4]] == std::vector<int> { 37, 49 });
}

TEST_CASE ("long notes swell with CC1 in crossfading presets and with CC11 in Velocity presets")
{
    Settings s;
    s.humanizeVelocity = 0.0;
    s.humanizeTimingMs = 0.0;
    s.intensity = 1.0;

    // A long note in the middle of a phrase.
    std::vector<Note> notes = { note (0, 480, 67), note (480, 3840, 69), note (4320, 480, 71), note (4800, 480, 72) };

    auto range = [] (const Part& p, int cc, Tick from, Tick to) {
        int lo = 127, hi = 0;
        for (const auto& c : p.controllers)
            if (c.controller == cc && c.tick >= from && c.tick <= to)
            {
                lo = std::min (lo, c.value);
                hi = std::max (hi, c.value);
            }
        return hi - lo;
    };

    s.presetType = PresetType::VelXF;
    const auto a = finesse (makePart (notes), violinProfile(), s);
    s.presetType = PresetType::Velocity;
    const auto b = finesse (makePart (notes), violinProfile(), s);

    // Look inside the long note, away from the transitions.
    CHECK (range (a.part, 1, 1200, 3000) > 0);
    CHECK (range (b.part, 11, 600, 3000) > 0);
}

TEST_CASE ("dynamic markings set the level: pp is softer than ff")
{
    Settings s;
    s.presetType = PresetType::VelXF;
    const auto part = makePart (line (16, 480));
    const Tick end = part.notes.back().end;

    const auto soft = finesse (part, violinProfile(), s, { { Marking::Kind::Dynamic, 0, end, 0 } });
    const auto loud = finesse (part, violinProfile(), s, { { Marking::Kind::Dynamic, 0, end, 5 } });
    for (size_t i = 0; i < soft.part.notes.size(); ++i)
        CHECK (controllerAt (soft.part, 1, soft.part.notes[i].start) + 40 < controllerAt (loud.part, 1, loud.part.notes[i].start));
}

TEST_CASE ("a crescendo grows across its passage and keeps its arrival level")
{
    Settings s;
    s.presetType = PresetType::VelXF;
    s.phraseArchAmount = 0.0;
    s.highLoudAmount = 0.0;
    s.durationContrastAmount = 0.0;

    std::vector<Note> notes;
    for (int i = 0; i < 24; ++i)
        notes.push_back (note (i * 480, 480, 72, 70));
    const auto part = makePart (notes);

    const auto plain = finesse (part, violinProfile(), s);
    const auto cresc = finesse (part, violinProfile(), s, { { Marking::Kind::Crescendo, 4 * 480, 12 * 480, 0 } });

    auto at = [] (const Result& r, size_t i) { return controllerAt (r.part, 1, r.part.notes[i].start); };
    CHECK (at (cresc, 2) == at (plain, 2));          // before: unchanged
    CHECK (at (cresc, 8) > at (plain, 8));           // during: louder
    CHECK (at (cresc, 11) > at (cresc, 6));          // growing
    CHECK (at (cresc, 20) >= at (plain, 20) + 20);   // after: stays at the arrival level
}

TEST_CASE ("human pauses: a bow jumping over a string gets a moment, and can be turned off")
{
    InstrumentProfile cello;
    cello.family = Family::Strings;
    cello.lowestNote = 36;
    cello.highestNote = 84;
    cello.openStrings = { 36, 43, 50, 57 }; // C G D A

    // C string (low C, D) to A string (A, B) and back, connected eighths.
    std::vector<Note> notes = { note (0, 240, 36), note (240, 240, 38), note (480, 240, 59), note (720, 240, 57),
                                note (960, 240, 38), note (1200, 240, 36), note (1440, 240, 38), note (1680, 240, 36) };
    Settings s;
    s.humanizeTimingMs = 0.0;
    const auto with = finesse (makePart (notes), cello, s);
    CHECK (with.part.notes[1].end < with.part.notes[2].start); // C string -> A string: a gap
    CHECK (with.part.notes[0].end > with.part.notes[1].start); // same string: connected
    CHECK (with.report.pauses >= 2);

    s.humanPauses = false;
    const auto without = finesse (makePart (notes), cello, s);
    CHECK (without.part.notes[1].end > without.part.notes[2].start);
    CHECK (without.report.pauses == 0);
}

TEST_CASE ("a new dynamic replaces the part of an old one it covers")
{
    std::vector<Marking> m;
    addMarking (m, { Marking::Kind::Dynamic, 0, 9600, 1 });      // p, whole part
    addMarking (m, { Marking::Kind::Dynamic, 1920, 3840, 4 });   // f, bars 2-3
    addMarking (m, { Marking::Kind::Crescendo, 3840, 5760, 0 }); // hairpins don't cut dynamics
    REQUIRE (m.size() == 4);
    CHECK (m[0].level == 1);
    CHECK (m[0].end == 1920);
    CHECK (m[1].level == 4);
    CHECK ((m[2].kind == Marking::Kind::Dynamic));
    CHECK (m[2].start == 3840);

    addMarking (m, { Marking::Kind::Diminuendo, 4800, 6000, 0 }); // replaces the crescendo
    int hairpins = 0;
    for (const auto& x : m)
        hairpins += x.kind != Marking::Kind::Dynamic;
    CHECK (hairpins == 1);

    clearMarkings (m, 0, 1000);
    CHECK (m.front().start >= 1000);
}

TEST_CASE ("guessProfile finds the instrument a name refers to, preferring the longest match")
{
    auto make = [] (std::string id, std::string name) {
        InstrumentProfile p;
        p.id = std::move (id);
        p.name = std::move (name);
        return p;
    };
    const std::vector<InstrumentProfile> profiles { make ("generic", "Generic (no range)"), make ("basses", "Basses"),
                                                    make ("clarinet", "Clarinet in Bb"), make ("bass-clarinet", "Bass Clarinet"),
                                                    make ("violins", "Violins") };
    auto id = [&] (const std::string& text) {
        const auto* p = guessProfile (text, profiles);
        return p != nullptr ? p->id : std::string ("none");
    };
    CHECK (id ("Violin I") == "violins");
    CHECK (id ("Double Bass") == "basses");
    CHECK (id ("Bass Clarinet") == "bass-clarinet"); // not "clarinet", whose name is longer
    CHECK (id ("Clarinet") == "clarinet");
    CHECK (id ("Piano") == "none");
    CHECK (id ("Generic") == "none");
}

// --- Picked articulations ----------------------------------------------------
// Keys as in the Studio Special Edition trumpet: C0..E0 (MIDI 24..28) pick the
// articulation (Staccato, Long notes, ...), C1/C#1 (36/37) the type
// (Sustain, Marcato).
namespace
{
std::vector<int> keyswitchPitchesAt (const Result& r, Tick from, Tick to)
{
    std::vector<int> v;
    for (const auto& k : r.part.keyswitches)
        if (k.start >= from && k.start < to)
            v.push_back (k.pitch);
    std::sort (v.begin(), v.end());
    return v;
}

ArticulationMarking pick (Tick start, Tick end, std::vector<int> keys, std::string name)
{
    return ArticulationMarking { start, end, std::move (keys), std::move (name) };
}
} // namespace

TEST_CASE ("a picked articulation writes its keyswitches ahead of its first note")
{
    auto notes = line (8, 480);
    ArticulationPicks picks;
    picks.markings = { pick (0, 8 * 480, { 25, 37 }, "Long notes > Marcato") };
    const auto r = finesse (makePart (notes), violinProfile(), Settings {}, {}, picks);
    REQUIRE_FALSE (r.part.notes.empty());

    REQUIRE (r.part.keyswitches.size() == 2);
    CHECK (r.report.keyswitchesWritten == 2);
    // At tick 0 they can't move earlier; the writer orders keyswitches first.
    for (const auto& k : r.part.keyswitches)
        CHECK (k.start <= r.part.notes[0].start);
    CHECK (r.articulationKeys[(size_t) r.articulationIndex[7]] == std::vector<int> { 25, 37 });
}

TEST_CASE ("after a picked passage the part switches back to the file's articulation")
{
    auto notes = line (12, 480);
    std::vector<Note> ks = { note (0, 30, 25), note (0, 30, 36) }; // Long notes > Sustain
    ArticulationPicks picks;
    picks.markings = { pick (4 * 480, 8 * 480, { 24 }, "Staccato") }; // notes 4-7
    const auto r = finesse (makePart (notes, ks), violinProfile(), Settings {}, {}, picks);

    auto keysOf = [&r] (size_t i) { return r.articulationKeys[(size_t) r.articulationIndex[i]]; };
    CHECK (keysOf (0) == std::vector<int> { 25, 36 });
    CHECK (keysOf (5) == std::vector<int> { 24, 36 }); // Staccato: the type level has no say
    CHECK (keysOf (8) == std::vector<int> { 25, 36 });
    // Only the articulation level is switched, and switched back.
    CHECK (keyswitchPitchesAt (r, 4 * 480 - 240, 4 * 480 + 1) == std::vector<int> { 24 });
    CHECK (keyswitchPitchesAt (r, 8 * 480 - 240, 8 * 480 + 1) == std::vector<int> { 25 });
    CHECK (r.report.keyswitchesWritten == 2);
}

TEST_CASE ("file keyswitches inside a picked passage are replaced, the rest are kept")
{
    auto notes = line (12, 480);
    std::vector<Note> ks = { note (0, 30, 25), note (0, 30, 36), note (6 * 480 - 10, 30, 26) };
    ArticulationPicks picks;
    picks.markings = { pick (4 * 480, 8 * 480, { 24 }, "Staccato") };
    const auto r = finesse (makePart (notes, ks), violinProfile(), Settings {}, {}, picks);

    CHECK (r.report.keyswitchesReplaced == 1);
    CHECK (keyswitchPitchesAt (r, 4 * 480 + 1, 8 * 480 - 240).empty());
    // The file had switched to D0 (26) inside the passage, so that's what follows it.
    CHECK (keyswitchPitchesAt (r, 8 * 480 - 240, 8 * 480 + 1) == std::vector<int> { 26 });
    CHECK (keyswitchPitchesAt (r, -1000, 1) == std::vector<int> { 25, 36 });
}

TEST_CASE ("without file keyswitches, a pick switches back to the player's start articulation")
{
    auto notes = line (12, 480);
    ArticulationPicks picks;
    picks.markings = { pick (4 * 480, 8 * 480, { 24 }, "Staccato") };
    picks.startKeys = { 25, 37 };
    const auto r = finesse (makePart (notes), violinProfile(), Settings {}, {}, picks);

    // The start articulation is set at the first note so playback is the same every time.
    CHECK (keyswitchPitchesAt (r, -1000, 1) == std::vector<int> { 25 });
    CHECK (keyswitchPitchesAt (r, 4 * 480 - 240, 4 * 480 + 1) == std::vector<int> { 24 });
    CHECK (keyswitchPitchesAt (r, 8 * 480 - 240, 8 * 480 + 1) == std::vector<int> { 25 });

    // Without a known start articulation the pick simply carries on.
    picks.startKeys.clear();
    const auto r2 = finesse (makePart (notes), violinProfile(), Settings {}, {}, picks);
    CHECK (r2.part.keyswitches.size() == 1);
}

TEST_CASE ("back-to-back picks only write the keys that change")
{
    auto notes = line (12, 480);
    ArticulationPicks picks;
    picks.markings = { pick (0, 4 * 480, { 25, 36 }, "Long notes > Sustain"),
                       pick (4 * 480, 12 * 480, { 25, 37 }, "Long notes > Marcato") };
    const auto r = finesse (makePart (notes), violinProfile(), Settings {}, {}, picks);
    CHECK (keyswitchPitchesAt (r, 4 * 480 - 240, 4 * 480 + 1) == std::vector<int> { 37 });
    CHECK (r.report.keyswitchesWritten == 3);
}

TEST_CASE ("no picks: keyswitches are kept exactly as written")
{
    auto notes = line (8, 480);
    std::vector<Note> ks = { note (0, 30, 25), note (1920, 30, 24) };
    const auto r = finesse (makePart (notes, ks), violinProfile(), Settings {}, {}, ArticulationPicks {});
    CHECK (r.part.keyswitches.size() == 2);
    CHECK (r.report.keyswitchesWritten == 0);
}

TEST_CASE ("a new pick replaces the part of an old one it covers")
{
    std::vector<ArticulationMarking> m;
    addArticulationMarking (m, pick (0, 1000, { 24 }, "Staccato"));
    addArticulationMarking (m, pick (400, 600, { 25, 37 }, "Long notes > Marcato"));
    REQUIRE (m.size() == 3);
    CHECK (m[0].end == 400);
    CHECK (m[1].name == "Long notes > Marcato");
    CHECK (m[2].start == 600);
    CHECK (m[2].name == "Staccato");

    clearArticulationMarkings (m, 450, 500);
    CHECK (m.size() == 2);
    addArticulationMarking (m, pick (0, 0, { 24 }, "empty")); // ignored
    CHECK (m.size() == 2);
}

TEST_CASE ("picks switch back by the profile's levels, not by octave")
{
    // A 13-key articulation level (24..36) and a Type level at 37/38, as
    // listed by a preset's tree.
    auto profile = violinProfile();
    for (int k = 24; k <= 36; ++k)
        profile.keys.push_back ({ k, 100 });
    profile.keys.push_back ({ 37, 101 });
    profile.keys.push_back ({ 38, 101 });

    auto notes = line (12, 480);
    std::vector<Note> ks = { note (0, 30, 25), note (0, 30, 37) };
    ArticulationPicks picks;
    picks.markings = { pick (4 * 480, 8 * 480, { 36 }, "Art 12") };
    const auto r = finesse (makePart (notes, ks), profile, Settings {}, {}, picks);

    // 36 is on the articulation level, so only 25 is written back after it
    // (37 on the Type level was never changed).
    CHECK (keyswitchPitchesAt (r, 4 * 480 - 240, 4 * 480 + 1) == std::vector<int> { 36 });
    CHECK (keyswitchPitchesAt (r, 8 * 480 - 240, 8 * 480 + 1) == std::vector<int> { 25 });
}

TEST_CASE ("an articulation's own layers beat the note-length rule")
{
    // Like Synchron's VelXF sus Violin I: Detache (C0 + D1) follows velocity
    // even when long; Sustain (C#0 + C1) crossfades even when short.
    auto profile = violinProfile();
    profile.articulations = { { { 24, 38 }, { "Short notes > Detache", 0.0, Articulation::Layers::Velocity } },
                              { { 25, 36 }, { "Long notes > Sustain", 0.0, Articulation::Layers::Crossfade } } };
    Settings s;
    s.presetType = PresetType::VelXFSustain;
    s.humanizeTimingMs = 0.0;

    std::vector<Note> notes;
    for (int i = 0; i < 4; ++i)
        notes.push_back (note (i * 960, 960, 72 + i, 80)); // long, Detache
    for (int i = 0; i < 8; ++i)
        notes.push_back (note (3840 + i * 240, 100, 72 + (i % 3), 80)); // short, Sustain
    std::vector<Note> ks = { note (0, 30, 24), note (0, 30, 38), note (3840 - 20, 10, 25), note (3840 - 20, 10, 36) };

    const auto out = finesse (makePart (notes, ks), profile, s).part.notes;
    REQUIRE (out.size() == 12);
    bool longsShaped = false;
    for (size_t i = 0; i < 4; ++i)
        longsShaped |= out[i].velocity != 80;
    CHECK (longsShaped);
    for (size_t i = 4; i < 12; ++i)
        CHECK (out[i].velocity == 80);

    // The Velocity preset type is the user's choice and wins.
    s.presetType = PresetType::Velocity;
    const auto v = finesse (makePart (notes, ks), profile, s);
    bool shortsShaped = false;
    for (size_t i = 4; i < 12; ++i)
        shortsShaped |= v.part.notes[i].velocity != 80;
    CHECK (shortsShaped);
}

TEST_CASE ("kept controllers stay, except the ones the engine writes itself")
{
    auto part = makePart (line (8, 480));
    part.controllers = { { 0, 1, 90, 1 }, { 0, 11, 100, 1 }, { 0, 20, 64, 1 }, { 960, 20, 0, 1 }, { 0, 64, 127, 1 } };
    auto count = [] (const Part& p, int cc) { return (int) controllersFor (p, cc).size(); };

    Settings s;
    s.presetType = PresetType::VelXFSustain;

    // Removed (the default): only the engine's own controllers are left.
    auto r = finesse (part, violinProfile(), s);
    CHECK (count (r.part, 20) == 0);
    CHECK (count (r.part, 64) == 0);
    CHECK (r.report.controllersRemoved == 5);
    CHECK (r.report.controllersKept == 0);

    // Kept: CC20 and CC64 stay exactly as written; CC1 and CC11 are the engine's lines.
    s.keepControllers = true;
    r = finesse (part, violinProfile(), s);
    CHECK (count (r.part, 20) == 2);
    CHECK (count (r.part, 64) == 1);
    CHECK (controllerAt (r.part, 20, 500) == 64);
    CHECK (controllerAt (r.part, 20, 1000) == 0);
    for (const auto& c : controllersFor (r.part, 1))
        CHECK_FALSE ((c.tick == 0 && c.value == 90)); // the file's CC1 is gone
    CHECK (r.report.controllersRemoved == 2);
    CHECK (r.report.controllersKept == 3);

    // Velocity presets: the engine writes no CC1, so the file's CC1 stays
    // (it can drive a crossfade there, e.g. Marcato's attack).
    s.presetType = PresetType::Velocity;
    r = finesse (part, violinProfile(), s);
    REQUIRE (count (r.part, 1) == 1);
    CHECK (controllerAt (r.part, 1, 0) == 90);
}

TEST_CASE ("kept controllers: the file's CC1 and CC11 win when the app's replacing is off")
{
    auto part = makePart (line (8, 480));
    part.controllers = { { 0, 1, 90, 1 }, { 960, 1, 40, 1 }, { 0, 20, 64, 1 } }; // CC1 and CC20, no CC11
    auto count = [] (const Part& p, int cc) { return (int) controllersFor (p, cc).size(); };

    Settings s;
    s.presetType = PresetType::VelXFSustain;
    s.keepControllers = true;
    s.replaceFileDynamics = false;

    const auto r = finesse (part, violinProfile(), s);
    CHECK (count (r.part, 1) == 2);                // the file's two events, exactly
    CHECK (controllerAt (r.part, 1, 0) == 90);
    CHECK (controllerAt (r.part, 1, 960) == 40);
    CHECK (count (r.part, 20) == 1);
    CHECK (count (r.part, 11) > 0);                // the file has no CC11, so the app's is written
    CHECK (r.report.controllersRemoved == 0);
    CHECK (r.report.controllersKept == 3);

    // Removing the file's controllers: nothing of the file's is left to win.
    s.keepControllers = false;
    const auto removed = finesse (part, violinProfile(), s);
    CHECK (count (removed.part, 20) == 0);
    CHECK (count (removed.part, 1) > 2);
}

// --- Crossfaded articulations -----------------------------------------------------
namespace
{
InstrumentProfile blendProfile (Articulation::Blend::Kind kind, int controller, double centre, Articulation::Layers layers)
{
    auto p = violinProfile();
    p.defaultArticulation.name = "Marcato";
    p.defaultArticulation.layers = layers;
    p.defaultArticulation.blend = { kind, controller, centre, "Attack" };
    return p;
}

// Four bars of quarter notes, a rest, then four fast notes.
std::vector<Note> accentLine()
{
    std::vector<Note> v;
    for (int i = 0; i < 16; ++i)
        v.push_back (note (i * 480, 400, 67 + (i % 4), 90));
    for (int i = 0; i < 4; ++i)
        v.push_back (note (9600 + i * 96, 90, 72, 90)); // sixteenth-note triplets at 120 bpm
    return v;
}
} // namespace

TEST_CASE ("an added layer (Marcato's attack) is an accent: more bite on strong beats and after rests, less in fast notes")
{
    Settings s;
    s.presetType = PresetType::VelXF;
    s.humanizeTimingMs = 0.0;
    const auto profile = blendProfile (Articulation::Blend::Kind::AddedLayer, -1, 60.0 / 127.0, Articulation::Layers::Crossfade);
    const auto r = finesse (makePart (accentLine()), profile, s);
    const auto& out = r.part.notes;
    REQUIRE (out.size() == 20);

    CHECK (out[4].velocity > out[5].velocity);   // bar 2 downbeat vs beat 2
    CHECK (out[16].velocity > out[18].velocity); // after the rest vs in the run
    for (const auto& n : out)
    {
        CHECK (n.velocity >= 10);
        CHECK (n.velocity <= 117);
    }
    REQUIRE (r.report.blends.size() == 1);
    CHECK (r.report.blends[0] == "Marcato: Attack per note (velocity)");

    // Character sets the range.
    auto spread = [&] (double intensity) {
        auto st = s;
        st.intensity = intensity;
        int lo = 127, hi = 0;
        for (const auto& n : finesse (makePart (accentLine()), profile, st).part.notes)
        {
            lo = std::min (lo, n.velocity);
            hi = std::max (hi, n.velocity);
        }
        return hi - lo;
    };
    CHECK (spread (0.3) < spread (1.0));
}

TEST_CASE ("an added layer on a CC is set before each note starts")
{
    Settings s;
    s.presetType = PresetType::Velocity; // Dim.Ctrl/A is CC1 in the Velocity presets
    const auto profile = blendProfile (Articulation::Blend::Kind::AddedLayer, 1, 60.0 / 127.0, Articulation::Layers::Velocity);
    const auto r = finesse (makePart (accentLine()), profile, s);
    const auto cc1 = controllersFor (r.part, 1);
    REQUIRE (cc1.size() >= 5);

    // Each note starts with its own value, set after the previous note began.
    const auto& notes = r.part.notes;
    for (size_t i = 1; i < notes.size(); ++i)
        CHECK (controllerAt (r.part, 1, notes[i].start) >= 0);
    for (const auto& c : cc1)
        for (size_t i = 0; i < notes.size(); ++i)
            if (c.tick < notes[i].start && (i == 0 || c.tick > notes[i - 1].start))
                CHECK (c.tick >= notes[i].start - r.part.tempo.ticksForMs ((double) notes[i].start, 20.0));
}

TEST_CASE ("blends never fight the dynamics or the user's own lines")
{
    Settings s;
    s.presetType = PresetType::VelXF; // CC1 is the dynamics here
    auto profile = blendProfile (Articulation::Blend::Kind::AddedLayer, 1, 0.5, Articulation::Layers::Crossfade);
    auto r = finesse (makePart (accentLine()), profile, s);
    CHECK (r.report.blends.empty());

    // Velocity is the dynamic in Velocity presets: a velocity blend steps aside.
    s.presetType = PresetType::Velocity;
    profile = blendProfile (Articulation::Blend::Kind::AddedLayer, -1, 0.5, Articulation::Layers::Velocity);
    CHECK (finesse (makePart (accentLine()), profile, s).report.blends.empty());

    // A CC20 line the user kept from the file wins over the app's.
    s.presetType = PresetType::VelXF;
    s.keepControllers = true;
    profile = blendProfile (Articulation::Blend::Kind::Crossfade, 20, 0.0, Articulation::Layers::Crossfade);
    auto part = makePart (accentLine());
    part.controllers = { { 0, 20, 33, 1 } };
    r = finesse (part, profile, s);
    REQUIRE (controllersFor (r.part, 20).size() == 1);
    CHECK (controllerAt (r.part, 20, 5000) == 33);
}

TEST_CASE ("a crossfade (XF tremolo) leans in towards the phrase peak and back")
{
    Settings s;
    s.presetType = PresetType::VelXF;
    s.humanizeTimingMs = 0.0;
    const auto profile = blendProfile (Articulation::Blend::Kind::Crossfade, 20, 0.0, Articulation::Layers::Crossfade);

    // One long phrase of half notes rising to a peak, then falling.
    std::vector<Note> notes;
    const int pitches[] = { 60, 62, 64, 67, 72, 69, 65, 62, 60 };
    for (int i = 0; i < 9; ++i)
        notes.push_back (note (i * 960, 940, pitches[i], 80));
    const auto r = finesse (makePart (notes), profile, s);
    REQUIRE (r.phrases.size() == 1);

    const auto& out = r.part.notes;
    const int start = controllerAt (r.part, 20, out[0].start);
    const int peak = controllerAt (r.part, 20, out[4].start + 400);
    const int end = controllerAt (r.part, 20, out[8].end - 10);
    CHECK (start <= 15);
    CHECK (peak > start + 20);
    CHECK (end < peak);
    CHECK (r.report.blends[0] == "Marcato: Attack follows the phrases (CC20)");
}

// --- Performance controls ----------------------------------------------------------
namespace
{
InstrumentProfile performProfile()
{
    auto p = violinProfile();
    p.attackControl = { 22, 0.0 };
    p.releaseControl = { 23, 0.5 };
    p.legatoBlurControl = { 33, 0.235 };
    return p;
}

// The value a controller has when note i starts.
int valueAtNote (const Result& r, int cc, size_t i) { return controllerAt (r.part, cc, r.part.notes[i].start); }
} // namespace

TEST_CASE ("release: notes ring at phrase ends and before rests, short detached notes stop cleanly")
{
    Settings s;
    s.humanizeTimingMs = 0.0;
    std::vector<Note> notes;
    for (int i = 0; i < 4; ++i)
        notes.push_back (note (i * 480, 470, 67 + i));        // a legato phrase...
    for (int i = 0; i < 4; ++i)
        notes.push_back (note (3840 + i * 240, 80, 72));       // ...a rest, then short detached notes...
    notes.push_back (note (6720, 960, 67));                    // ...and a final note
    const auto r = finesse (makePart (notes), performProfile(), s);
    REQUIRE (r.part.notes.size() == 9);

    const int mid = valueAtNote (r, 23, 1), phraseEnd = valueAtNote (r, 23, 3);
    const int detached = valueAtNote (r, 23, 5), last = valueAtNote (r, 23, 8);
    CHECK (mid == 64);              // the preset's own release
    CHECK (phraseEnd > mid);
    CHECK (detached < mid);
    CHECK (last > phraseEnd);
}

TEST_CASE ("attack: softer for quiet entries only")
{
    Settings s;
    std::vector<Marking> markings = { { Marking::Kind::Dynamic, 0, 1920, 0 },        // pp
                                      { Marking::Kind::Dynamic, 3840, 5760, 5 } };   // ff
    std::vector<Note> notes;
    for (int i = 0; i < 4; ++i)
        notes.push_back (note (i * 480, 470, 67 + i));
    for (int i = 0; i < 4; ++i)
        notes.push_back (note (3840 + i * 480, 470, 67 + i));
    const auto r = finesse (makePart (notes), performProfile(), s, markings);

    CHECK (valueAtNote (r, 22, 0) > 15); // the pp entry comes out of nothing
    CHECK (valueAtNote (r, 22, 1) == 0); // the preset's attack after it
    CHECK (valueAtNote (r, 22, 4) == 0); // the ff entry keeps it
}

TEST_CASE ("legato blur: only at joins, smoother when slow and soft, cleaner when fast")
{
    Settings s;
    s.humanizeTimingMs = 0.0;
    std::vector<Note> notes;
    for (int i = 0; i < 4; ++i)
        notes.push_back (note (i * 1440, 1440, 60 + 2 * i));        // slow connected notes
    for (int i = 0; i < 8; ++i)
        notes.push_back (note (7680 + i * 96, 96, 64 + (i % 3)));   // a fast connected run, after a rest
    const auto r = finesse (makePart (notes), performProfile(), s);

    const int slow = valueAtNote (r, 33, 2), fast = valueAtNote (r, 33, 8);
    CHECK (slow > 30);   // preset 0.235 = 30
    CHECK (fast < slow);
    // Nothing at the first note: there's no join to blur.
    for (const auto& c : controllersFor (r.part, 33))
        CHECK (c.tick > r.part.notes[0].start);
}

TEST_CASE ("performance controls: each is set before its note, steps aside for kept file controllers, and needs the preset")
{
    Settings s;
    auto notes = line (8, 480);
    auto r = finesse (makePart (notes), performProfile(), s);
    for (const auto& c : controllersFor (r.part, 23))
    {
        // Every release value lands in the gap before a note starts.
        const bool beforeANote = std::any_of (r.part.notes.begin(), r.part.notes.end(), [&] (const Note& n) {
            return c.tick <= n.start && c.tick >= n.start - r.part.tempo.ticksForMs ((double) n.start, 20.0);
        });
        CHECK (beforeANote);
    }

    s.keepControllers = true;
    auto part = makePart (notes);
    part.controllers = { { 0, 23, 100, 1 } };
    r = finesse (part, performProfile(), s);
    REQUIRE (controllersFor (r.part, 23).size() == 1);
    CHECK (controllersFor (r.part, 23)[0].value == 100);
    CHECK (! controllersFor (r.part, 22).empty()); // the others are still shaped

    CHECK (controllersFor (finesse (makePart (notes), violinProfile(), Settings {}).part, 23).empty());
}
