#include <doctest/doctest.h>

#include "TestHelpers.h"

#include <BuiltinProfiles.h>
#include <JsonIO.h>
#include <MidiIO.h>
#include <SynchronState.h>

using namespace vslf;
using namespace vslf::test;

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
    p.articulations = { { { 36, 49 }, { "Legato", -20.0 } } };
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
    CHECK (back.articulations[0].articulation.name == "Legato");
    CHECK (back.articulations[0].articulation.attackOffsetMs == doctest::Approx (-20.0));
}

TEST_CASE ("bad profiles are rejected with a message")
{
    juce::var json;
    REQUIRE (juce::JSON::parse (R"({ "articulations": [ { "keys": ["H9"], "name": "X" } ] })", json).wasOk());
    InstrumentProfile p;
    std::string error;
    CHECK_FALSE (io::profileFromJson (json, p, error));
    CHECK (error.find ("H9") != std::string::npos);
}

TEST_CASE ("settings round-trip through JSON")
{
    Settings s;
    s.intensity = 1.25;
    s.seed = 42;
    s.useInputVelocity = false;
    s.chords = ChordResolution::KeepLowest;
    s.presetType = PresetType::Velocity;

    Settings back;
    io::settingsFromJson (io::settingsToJson (s), back);
    CHECK (back.intensity == doctest::Approx (1.25));
    CHECK (back.seed == 42);
    CHECK_FALSE (back.useInputVelocity);
    CHECK (back.chords == ChordResolution::KeepLowest);
    CHECK ((back.presetType == PresetType::Velocity)); // extra parentheses: doctest would pick up vslf::toString
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

TEST_CASE ("markings round-trip through JSON")
{
    std::vector<Marking> m = { { Marking::Kind::Dynamic, 0, 1920, 1 }, { Marking::Kind::Crescendo, 1920, 3840, 0 } };
    const auto back = io::markingsFromJson (io::markingsToJson (m));
    REQUIRE (back.size() == 2);
    CHECK ((back[0].kind == Marking::Kind::Dynamic));
    CHECK (back[0].level == 1);
    CHECK ((back[1].kind == Marking::Kind::Crescendo));
    CHECK (back[1].end == 3840);
}

// --- Synchron Player state -----------------------------------------------------
// Shaped like Synchron Player's Audio Unit state: a binary plist whose
// PluginState is Zstandard-compressed JSON. Made with Python's plistlib and
// zstandard, so the reader is checked against another implementation.
namespace
{
    juce::MemoryBlock fromBase64 (const char* text)
    {
        juce::MemoryOutputStream out;
        REQUIRE (juce::Base64::convertFromBase64 (out, text));
        return out.getMemoryBlock();
    }

    // Empty Flow view (no preset loaded).
    const char* synchronEmptyFlow =
        "YnBsaXN0MDDYAQIDBAUGBwgJCgsQERITFFtQbHVnaW5TdGF0ZVRkYXRhXGVsZW1lbnQtbmFtZVxtYW51ZmFjdHVyZXJUbmFtZVdzdWJ0eXBlVHR5cGVXdmVyc2lvbk8Q"
        "aCi1L/0gdP0CAPKFFBqAuRkD3ohEM7bhi9S+mANolCE5WhcxczCjI83gSFGVUvG11gwggOeeQ32GIcBXgmgm3y5DePheKDsYR4ooKEMy8T2+UmUiLJQpseMFP+sYXgID"
        "ACqhFmXcPsw8TxAoAAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJ9EMDVEy0Q4PUTBXT3V0IDEvMhJWU0xTWFVudGl0bGVkElZJTjASYXVtdRAA"
        "AAgAGQAlACoANwBEAEkAUQBWAF4AyQD0APcA+QD8AP4BBgELARQBGQEeAAAAAAAAAgEAAAAAAAAAFQAAAAAAAAAAAAAAAAAAASA=";

    // Flow view with a Flow preset loaded.
    const char* synchronFlowPreset =
        "YnBsaXN0MDDYAQIDBAUGBwgJCgsQERITFFtQbHVnaW5TdGF0ZVRkYXRhXGVsZW1lbnQtbmFtZVxtYW51ZmFjdHVyZXJUbmFtZVdzdWJ0eXBlVHR5cGVXdmVyc2lvbk8Q"
        "hii1L/0go+0DANLHGh1gbXOVvu3MkBdNahdmu9Xz//1acgspP4ZkGgQ5CFHlu+1zCnmKMaqAkldmzXtalkEePs7q88QMdTaeHmnMPN/rWAs5+nh6IenltonG7Tm9Dq5E"
        "UQLCWigOBgbwPNSnDgFbnYOaR3kGGeNRBQCqUKkwbhOGP5NH3ZYyTxAoAAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJ9EMDVEy0Q4PUTBXT3V0"
        "IDEvMhJWU0xTWFVudGl0bGVkElZJTjASYXVtdRAAAAgAGQAlACoANwBEAEkAUQBWAF4A5wESARUBFwEaARwBJAEpATIBNwE8AAAAAAAAAgEAAAAAAAAAFQAAAAAAAAAA"
        "AAAAAAAAAT4=";
}

TEST_CASE ("Synchron Player state: the JSON inside is readable")
{
    const auto json = io::synchronStateJson (fromBase64 (synchronEmptyFlow));
    REQUIRE (json.has_value());
    CHECK (juce::JSON::parse (*json)["data"]["meta"]["gui"]["currentSkin"].toString() == "Flow");
    CHECK (juce::JSON::parse (*json)["data"]["meta"]["gui"]["height"] == juce::var (855));
}

TEST_CASE ("Synchron Player state: an empty Flow view switches to Precision, and nothing else changes")
{
    const auto original = fromBase64 (synchronEmptyFlow);
    const auto switched = io::withPrecisionView (original);
    REQUIRE (switched.has_value());

    const auto before = io::synchronStateJson (original);
    const auto after = io::synchronStateJson (*switched);
    REQUIRE (after.has_value());
    CHECK (*after == before->replace ("\"currentSkin\":\"Flow\"", "\"currentSkin\":\"SynchronPlayer\""));

    // Already Precision: left alone.
    CHECK (! io::withPrecisionView (*switched).has_value());
}

TEST_CASE ("Synchron Player state: a loaded Flow preset and other plug-ins are left alone")
{
    CHECK (! io::withPrecisionView (fromBase64 (synchronFlowPreset)).has_value());

    const juce::String other ("<plugin state that isn't a property list>");
    CHECK (! io::withPrecisionView (juce::MemoryBlock (other.toRawUTF8(), other.getNumBytesAsUTF8())).has_value());
    CHECK (! io::synchronStateJson ({}).has_value());
}

TEST_CASE ("Synchron Player preset paths give the instrument and the preset folder")
{
    auto p = io::describeSynchronPreset ("Studio Special Editions/01 Solo strings/02 VelXF - MOD/01 Violin I - VelXF");
    CHECK (p.instrument == "Violin I");
    REQUIRE (p.type.has_value());
    CHECK ((*p.type == PresetType::VelXF));

    p = io::describeSynchronPreset ("Studio Special Editions/03 Brass/01 VelXF sus - MOD/01 Trumpet in C - VelXF sus");
    CHECK (p.instrument == "Trumpet in C");
    REQUIRE (p.type.has_value());
    CHECK ((*p.type == PresetType::VelXFSustain));

    p = io::describeSynchronPreset ("Studio Special Editions/01 Solo strings/03 Velocity/04 Double Bass - Velocity");
    CHECK (p.instrument == "Double Bass");
    REQUIRE (p.type.has_value());
    CHECK ((*p.type == PresetType::Velocity));

    // Flow presets have no preset folder.
    p = io::describeSynchronPreset ("$FLOW_SE/01 Strings/01 Solo Strings/01 Violin I");
    CHECK (p.instrument == "Violin I");
    CHECK (! p.type.has_value());

    CHECK (io::describeSynchronPreset ("").instrument.isEmpty());
}

TEST_CASE ("Synchron Player instrument names pick the built-in profiles")
{
    const auto profiles = io::builtinProfiles();
    auto id = [&] (const char* name) {
        const auto* p = guessProfile (name, profiles);
        return p != nullptr ? p->id : std::string ("none");
    };
    CHECK (id ("Violin I") == "violins");
    CHECK (id ("Viola") == "violas");
    CHECK (id ("Cello") == "cellos");
    CHECK (id ("Double Bass") == "basses");
    CHECK (id ("Trumpet in C") == "trumpet");
    CHECK (id ("Horn") == "horn");
    CHECK (id ("Tenor Trombone") == "trombone");
    CHECK (id ("Bass Trombone") == "bass-trombone");
    CHECK (id ("English Horn") == "english-horn");
    CHECK (id ("Bass Clarinet") == "bass-clarinet");
    CHECK (id ("Contrabassoon") == "contrabassoon");
    CHECK (id ("Bassoon") == "bassoon");
}

TEST_CASE ("Synchron Player state: the loaded preset is read from it")
{
    const auto preset = io::synchronPreset (fromBase64 (synchronFlowPreset));
    REQUIRE (preset.has_value());
    CHECK (preset->instrument == "Violin I");
    CHECK (io::synchronPreset (fromBase64 (synchronEmptyFlow))->path.isEmpty());
}

// The articulation tree of the Studio Special Edition trumpet (VelXF - MOD),
// cut down from Synchron Player 1.1.0's state with Long notes > Marcato
// selected: just the fields the reader uses.
namespace
{
    const char* trumpetTree = R"({"version":0,"data":{"currentPreset":"Studio Special Editions/22 Trumpets/02 VelXF - MOD/02 Trumpet I (C) - VelXF","custom":{"sampler":{"rootNode":{"title":"","subTitle":"Articulation","switchControl":25,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Staccato","patchName":"01 TrC staccato"},{"title":"Long notes","subTitle":"Type","switchControl":37,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Sustain","patchName":"04 TrC sustain"},{"title":"Marcato","subTitle":"Attack","switchControl":257,"whiteNoteSwitch":false,"currentNodeIndex":0.97058,"nodes":[{"title":"Sustain","patchName":"04 TrC sustain"},{"title":"Staccato","patchName":"01 TrC staccato"}]}]},{"title":"Legato","subTitle":"Type","switchControl":37,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Legato","patchName":"11 TrC legato"},{"title":"Legato-sus","patchName":"12 TrC legato-sus"}]},{"title":"Sforzato","patchName":"06 TrC sfz"},{"title":"Custom","subTitle":"Type","switchControl":37,"whiteNoteSwitch":false,"currentNodeIndex":1.0,"nodes":[{"title":"Custom 1","patchName":""},{"title":"Custom 2","patchName":""},{"title":"Custom 3","patchName":""},{"title":"Custom 4","patchName":""}]}]}}}}})";
}

TEST_CASE ("Synchron Player state: the articulation tree gives the articulations and their keys")
{
    const auto a = io::synchronArticulationsFromJson (juce::JSON::parse (trumpetTree));

    std::vector<std::pair<juce::String, std::vector<int>>> got;
    for (const auto& c : a.choices)
        got.push_back ({ c.name(), c.keys });

    // Synchron's C1 is MIDI 24 (Logic's C0). Marcato's Attack level is picked
    // by velocity, so Marcato is one articulation. Custom has no patches.
    const std::vector<std::pair<juce::String, std::vector<int>>> expected = {
        { "Staccato", { 24 } },
        { "Long notes > Sustain", { 25, 36 } },
        { "Long notes > Marcato", { 25, 37 } },
        { "Legato > Legato", { 26, 36 } },
        { "Legato > Legato-sus", { 26, 37 } },
        { "Sforzato", { 27 } },
    };
    CHECK (got == expected);
    CHECK (a.currentKeys == std::vector<int> { 25, 37 });

    // The Staccato state differs only in the root's selection.
    const auto staccato = io::synchronArticulationsFromJson (
        juce::JSON::parse (juce::String (trumpetTree).replace (R"("switchControl":25,"whiteNoteSwitch":false,"currentNodeIndex":1.0)",
                                                               R"("switchControl":25,"whiteNoteSwitch":false,"currentNodeIndex":0.0)")));
    CHECK (staccato.currentKeys == std::vector<int> { 24 });
}

TEST_CASE ("Synchron Player state: white-key levels skip the black keys, and no tree gives nothing")
{
    const auto a = io::synchronArticulationsFromJson (juce::JSON::parse (
        R"({"data":{"custom":{"sampler":{"rootNode":{"switchControl":25,"whiteNoteSwitch":true,"currentNodeIndex":2,"nodes":[)"
        R"({"title":"A","patchName":"a"},{"title":"B","patchName":"b"},{"title":"C","patchName":"c"},{"title":"D","patchName":"d"}]}}}}})"));
    REQUIRE (a.choices.size() == 4);
    CHECK (a.choices[1].keys == std::vector<int> { 26 });
    CHECK (a.choices[3].keys == std::vector<int> { 29 });
    CHECK (a.currentKeys == std::vector<int> { 28 });

    CHECK (io::synchronArticulationsFromJson (juce::JSON::parse (R"({"data":{}})")).choices.empty());
}

TEST_CASE ("picked articulations round-trip through JSON")
{
    std::vector<ArticulationMarking> m = { { 0, 960, { 25, 37 }, "Long notes > Marcato" }, { 960, 1920, { 24 }, "Staccato" } };
    const auto back = io::articulationMarkingsFromJson (juce::JSON::parse (juce::JSON::toString (io::articulationMarkingsToJson (m))));
    REQUIRE (back.size() == 2);
    CHECK (back[0].keys == std::vector<int> { 25, 37 });
    CHECK (back[0].name == "Long notes > Marcato");
    CHECK (back[1].start == 960);
    CHECK (back[1].end == 1920);
}
