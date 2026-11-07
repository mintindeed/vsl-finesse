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
