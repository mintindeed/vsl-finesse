#include <doctest/doctest.h>

#include "TestHelpers.h"

#include <BuiltinProfiles.h>
#include <JsonIO.h>
#include <MidiIO.h>

using namespace vslp;
using namespace vslp::test;

TEST_CASE ("MIDI files round-trip notes, keyswitches, tempo and meter")
{
    auto part = makePart (line (8, 240), {});
    part.keyswitches = { note (0, 30, 36) };
    part.tempo = TempoMap (480, { { 0, 600000.0 }, { 1920, 400000.0 } });
    part.timeSignatures = { { 0, 3, 4 } };
    part.controllers = { { 0, 1, 70, 1 } };

    juce::TemporaryFile temp (".mid");
    std::string error;
    REQUIRE (io::writeMidiFile (part, temp.getFile(), error));

    const auto read = io::readMidiFile (temp.getFile());
    REQUIRE (read.ok);
    const auto& back = read.part;
    CHECK (back.name == "Test");
    CHECK (back.ppq == 480);
    CHECK (back.notes.size() == 9); // 8 notes + the keyswitch (separated later by ingest)
    CHECK (back.controllers.size() == 1);
    REQUIRE (back.timeSignatures.size() == 1);
    CHECK (back.timeSignatures[0].numerator == 3);
    REQUIRE (back.tempo.changes().size() == 2);
    CHECK (back.tempo.changes()[1].tick == 1920);
    CHECK (back.tempo.changes()[1].microsecondsPerQuarter == doctest::Approx (400000.0));
}

TEST_CASE ("finessed output reads back with the same notes and keyswitches")
{
    auto notes = line (16, 240);
    auto part = makePart (notes, { note (0, 30, 36), note (1920, 30, 49) });
    const auto result = finesse (part, violinProfile(), Settings {});

    juce::TemporaryFile temp (".mid");
    std::string error;
    REQUIRE (io::writeMidiFile (result.part, temp.getFile(), error));
    const auto read = io::readMidiFile (temp.getFile());
    REQUIRE (read.ok);

    Report report;
    const auto again = ingest (read.part, violinProfile(), Settings {}, report);
    CHECK (again.notes.size() == result.part.notes.size());
    CHECK (again.keyswitches.size() == 2);
    CHECK (read.part.controllers.size() == result.part.controllers.size());
}

TEST_CASE ("finessed file name")
{
    const auto f = juce::File::getCurrentWorkingDirectory().getChildFile ("Melody.mid");
    CHECK (io::finessedFileFor (f).getFileName() == "Melody (Finessed).mid");
}

TEST_CASE ("profiles round-trip through JSON, with note names")
{
    InstrumentProfile p = violinProfile();
    p.name = "Violins";
    p.keys = { { 36, 0 }, { 49, 1 } };
    p.articulations = { { { 36, 49 }, { "Legato", DynamicsMode::VelXFSustain, true, -20.0 } } };
    p.maxBreathSeconds = 0.0;

    const auto json = io::profileToJson (p);
    CHECK (json["range"][0].toString() == "G2");

    InstrumentProfile back;
    std::string error;
    REQUIRE (io::profileFromJson (json, back, error));
    CHECK (back.lowestNote == 55);
    CHECK (back.keys.size() == 2);
    REQUIRE (back.articulations.size() == 1);
    CHECK (back.articulations[0].keys == std::vector<int> { 36, 49 });
    CHECK (back.articulations[0].articulation.legato);
    CHECK (back.articulations[0].articulation.attackOffsetMs == doctest::Approx (-20.0));
}

TEST_CASE ("bad profiles are rejected with a message")
{
    juce::var json;
    REQUIRE (juce::JSON::parse (R"({ "articulations": [ { "keys": ["C1"], "name": "X", "mode": "loud" } ] })", json).wasOk());
    InstrumentProfile p;
    std::string error;
    CHECK_FALSE (io::profileFromJson (json, p, error));
    CHECK (error.find ("loud") != std::string::npos);
}

TEST_CASE ("settings round-trip through JSON")
{
    Settings s;
    s.intensity = 1.25;
    s.seed = 42;
    s.useInputVelocity = false;
    s.chords = ChordResolution::KeepLowest;

    Settings back;
    io::settingsFromJson (io::settingsToJson (s), back);
    CHECK (back.intensity == doctest::Approx (1.25));
    CHECK (back.seed == 42);
    CHECK_FALSE (back.useInputVelocity);
    CHECK (back.chords == ChordResolution::KeepLowest);
}

TEST_CASE ("built-in profiles load")
{
    const auto profiles = io::builtinProfiles();
    CHECK (profiles.size() >= 18);
    REQUIRE (io::findProfile (profiles, "generic") != nullptr);
    const auto* violins = io::findProfile (profiles, "violins");
    REQUIRE (violins != nullptr);
    CHECK (violins->lowestNote == 55);
    const auto* flute = io::findProfile (profiles, "flute");
    REQUIRE (flute != nullptr);
    CHECK (flute->maxBreathSeconds > 0.0);
}
