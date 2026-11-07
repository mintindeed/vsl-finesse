#include <doctest/doctest.h>

#include "TestHelpers.h"

#include <BuiltinProfiles.h>
#include <JsonIO.h>
#include <MidiIO.h>
#include <SynchronState.h>

#include "SynchronTrees.h"

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
    CHECK (id (io::describeSynchronPreset ("Studio Special Editions/01 Solo strings/01 VelXF sus - MOD/01 Violin I PLUS - VelXF sus")
                   .instrument.toRawUTF8()) == "violins");
    CHECK (id (io::describeSynchronPreset ("Studio Special Editions/22 Trumpets/02 VelXF - MOD/02 Trumpet I (C) - VelXF")
                   .instrument.toRawUTF8()) == "trumpet");
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

TEST_CASE ("Synchron Player tree: keys get their tree level, picks are looked up by name")
{
    // An articulation level with 13 keys runs past an octave: C0..C1 (24..36)
    // are all the first level, and the Type level starts at C#1 (37).
    io::SynchronArticulations tree;
    for (int i = 0; i < 13; ++i)
        tree.choices.push_back ({ juce::StringArray { "Art " + juce::String (i) }, { 24 + i } });
    tree.choices.push_back ({ juce::StringArray { "Long notes", "Sustain" }, { 25, 37 } });
    tree.choices.push_back ({ juce::StringArray { "Long notes", "Marcato" }, { 25, 38 } });

    const auto profile = io::withTree (InstrumentProfile {}, tree.choices);
    CHECK (profile.levelOf (24) == profile.levelOf (36));
    CHECK (profile.levelOf (37) != profile.levelOf (36));
    CHECK (profile.levelOf (37) == profile.levelOf (38));
    CHECK (profile.isKeyswitch (36));
    CHECK (profile.resolve ({ 25, 38 }).name == "Long notes > Marcato");

    // The same name in another preset can have other keys; a name it doesn't have is left out.
    std::vector<ArticulationMarking> picks = { { 0, 960, { 99 }, "Long notes > Marcato" }, { 960, 1920, { 24 }, "Flautando" } };
    std::vector<std::string> missing;
    const auto resolved = io::resolvePicks (picks, tree.choices, missing);
    REQUIRE (resolved.size() == 1);
    CHECK (resolved[0].keys == std::vector<int> { 25, 38 });
    CHECK (missing == std::vector<std::string> { "Flautando" });

    // No tree (another instrument): picks keep their keys.
    CHECK (io::resolvePicks (picks, {}, missing).size() == 2);
}

TEST_CASE ("Synchron Player state: a three-level tree gives three keys per articulation")
{
    const auto a = io::synchronArticulationsFromJson (juce::JSON::parse (
        R"({"data":{"custom":{"sampler":{"rootNode":{"switchControl":25,"currentNodeIndex":0,"nodes":[)"
        R"({"title":"Long notes","switchControl":37,"currentNodeIndex":0,"nodes":[)"
        R"({"title":"Sustain","switchControl":49,"currentNodeIndex":1,"nodes":[{"title":"Vibrato","patchName":"v"},{"title":"Non vibrato","patchName":"nv"}]},)"
        R"({"title":"Marcato","patchName":"m"}]}]}}}}})"));
    REQUIRE (a.choices.size() == 3);
    CHECK (a.choices[1].name() == "Long notes > Sustain > Non vibrato");
    CHECK (a.choices[1].keys == std::vector<int> { 24, 36, 49 });
    CHECK (a.choices[2].keys == std::vector<int> { 24, 37 });
    CHECK (a.currentKeys == std::vector<int> { 24, 36, 49 });
}

// --- Real Special Edition trees (tests/SynchronTrees.h) ---------------------------
namespace
{
    io::SynchronArticulations treeOf (const char* json) { return io::synchronArticulationsFromJson (juce::JSON::parse (json)); }

    const io::SynchronArticulation* find (const io::SynchronArticulations& a, const char* name)
    {
        for (const auto& c : a.choices)
            if (c.name() == name)
                return &c;
        return nullptr;
    }

    using Layers = Articulation::Layers;
}

TEST_CASE ("Violin I VelXF sus: white-key Type level, and per-patch layers")
{
    const auto a = treeOf (trees::violinVelXFSus);
    juce::StringArray names;
    for (const auto& c : a.choices)
        names.add (c.name());
    CHECK (names.joinIntoString (", ")
           == "Short notes > Staccato, Short notes > Detache, Long notes > Sustain, Long notes > Marcato, "
              "Long notes > XF tremolo, Legato > Legato, Legato > Portamento, Dynamics > Sforzato, "
              "Dynamics > Sfz XF tremolo, Tremolo > Tremolo, Tremolo > Trem. marcato, Pizzicato");

    // Short notes uses white keys: Detache is D1 (38), not C#1.
    CHECK (find (a, "Short notes > Detache")->keys == std::vector<int> { 24, 38 });
    CHECK (find (a, "Long notes > Marcato")->keys == std::vector<int> { 25, 37 });
    CHECK (find (a, "Pizzicato")->keys == std::vector<int> { 29 });
    CHECK (a.currentKeys == std::vector<int> { 25, 38 }); // Long notes > XF tremolo

    // Staccato and Detache follow velocity at any length; sustained patches crossfade.
    CHECK (find (a, "Short notes > Staccato")->layers == Layers::Velocity);
    CHECK (find (a, "Short notes > Detache")->layers == Layers::Velocity);
    CHECK (find (a, "Long notes > Sustain")->layers == Layers::Crossfade);
    CHECK (find (a, "Long notes > Marcato")->layers == Layers::Crossfade);
}

TEST_CASE ("Violin I PLUS Velocity: more articulations, all following velocity")
{
    const auto a = treeOf (trees::violinPlusVelocity);
    CHECK (a.choices.size() == 31);
    CHECK (find (a, "Tremolo + Trills > Fast rep. 190")->keys == std::vector<int> { 28, 42 });
    CHECK (find (a, "Sul ponticello > Tremolo")->keys == std::vector<int> { 31, 39 });
    CHECK (find (a, "Custom > Custom 1") == nullptr);
    for (const auto& c : a.choices)
        CHECK (c.layers == Layers::Velocity);

    // Same name, other keys than in the VelXF sus preset: Detache is a third
    // Short notes patch here, after Detache short.
    CHECK (find (a, "Short notes > Detache")->keys == std::vector<int> { 24, 38 });
    CHECK (find (a, "Short notes > Detache short")->keys == std::vector<int> { 24, 37 });
    CHECK (find (a, "Pizzicato + Legno > Pizzicato")->keys == std::vector<int> { 29, 36 });
}

TEST_CASE ("Double basses VelXF: keyswitches above the range")
{
    const auto a = treeOf (trees::doubleBassesVelXF);
    CHECK (find (a, "Short notes > Staccato")->keys == std::vector<int> { 84, 96 });
    CHECK (find (a, "Short notes > Detache")->keys == std::vector<int> { 84, 98 });
    CHECK (find (a, "Long notes > Marcato")->keys == std::vector<int> { 85, 97 });
    CHECK (find (a, "Pizzicato")->keys == std::vector<int> { 89 });
    for (const auto& c : a.choices)
        CHECK (c.layers == Layers::Crossfade);

    // The basses' range (Logic's B-1 to A3) leaves the keys as keyswitches.
    const auto builtins = io::builtinProfiles();
    const auto basses = std::find_if (builtins.begin(), builtins.end(), [] (const InstrumentProfile& p) { return p.id == "basses"; });
    REQUIRE (basses != builtins.end());
    const auto profile = io::withTree (*basses, a.choices);
    CHECK (profile.isKeyswitch (84));
    CHECK (profile.levelOf (84) != profile.levelOf (96));
}

TEST_CASE ("Timpani VelXF rolls and Harp: one level, and the preset type from the state")
{
    const auto timpani = treeOf (trees::timpaniVelXFRolls);
    REQUIRE (timpani.choices.size() == 3);
    CHECK (find (timpani, "Single hits")->layers == Layers::Velocity);
    CHECK (find (timpani, "Secco")->layers == Layers::Velocity);
    CHECK (find (timpani, "Rolls")->layers == Layers::Crossfade);
    CHECK (find (timpani, "Rolls")->keys == std::vector<int> { 26 });

    const auto harp = treeOf (trees::harp);
    REQUIRE (harp.choices.size() == 2);
    CHECK (find (harp, "Regular")->keys == std::vector<int> { 21 }); // Logic's A-1, below the harp's range
    CHECK (find (harp, "Harmonics")->keys == std::vector<int> { 22 });
    CHECK (find (harp, "Harmonics")->layers == Layers::Velocity);
    CHECK (harp.currentKeys == std::vector<int> { 22 });

    // No preset folder in the path: the state decides.
    CHECK_FALSE (io::describeSynchronPreset ("Studio Special Editions/06 Plucked instruments/01 Harp I").type.has_value());
}

TEST_CASE ("the start articulation names the notes before the first keyswitch")
{
    const auto a = treeOf (trees::violinVelXFSus);
    const auto profile = io::withTree (InstrumentProfile {}, a.choices, { 24, 36 });
    CHECK (profile.defaultArticulation.name == "Short notes > Staccato");
    CHECK (profile.defaultArticulation.layers == Layers::Velocity);
}

TEST_CASE ("crossfaded dimensions: the kind from Mix/XFade, the control's source from the preset")
{
    using Kind = Articulation::Blend::Kind;

    // VelXF sus: Marcato's Attack adds a staccato attack (Sustain is Mix),
    // driven by velocity; XF tremolo crossfades (both XFade) with CC20.
    const auto sus = treeOf (trees::violinVelXFSus);
    const auto* marcato = find (sus, "Long notes > Marcato");
    CHECK (marcato->blend.kind == Kind::AddedLayer);
    CHECK (marcato->blend.controller == -1);
    CHECK (marcato->blend.name == "Attack");
    CHECK (marcato->blendSlot == 0);
    const auto* tremolo = find (sus, "Long notes > XF tremolo");
    CHECK (tremolo->blend.kind == Kind::Crossfade);
    CHECK (tremolo->blend.controller == 20);
    CHECK (tremolo->blendSlot == 1);
    CHECK (find (sus, "Dynamics > Sfz XF tremolo")->blend.kind == Kind::Crossfade);
    CHECK (find (sus, "Tremolo > Trem. marcato")->blend.kind == Kind::AddedLayer);
    CHECK (find (sus, "Long notes > Sustain")->blend.kind == Kind::None);
    CHECK (sus.controlValues.at (0) == doctest::Approx (60.0 / 127.0).epsilon (0.01));
    CHECK (sus.controlValues.at (1) == 0.0);

    // The Velocity preset sends Dim.Ctrl/A to CC1 instead.
    const auto plus = treeOf (trees::violinPlusVelocity);
    CHECK (find (plus, "Long notes > Marcato")->blend.controller == 1);
    CHECK (find (plus, "Harmonics > Harm. marcato")->blend.kind == Kind::AddedLayer);

    CHECK (find (treeOf (trees::trumpetVelXF), "Long notes > Marcato")->blend.controller == -1);
    for (const auto& c : treeOf (trees::harp).choices)
        CHECK (c.blend.kind == Kind::None);

    // withTree centres each blend on the preset's value for its control.
    const auto profile = io::withTree (InstrumentProfile {}, sus.choices, {}, sus.controlValues);
    CHECK (profile.resolve (marcato->keys).blend.centre == doctest::Approx (60.0 / 127.0).epsilon (0.01));
    CHECK (profile.resolve (tremolo->keys).blend.centre == 0.0);
    CHECK (profile.resolve (tremolo->keys).blend.controller == 20);
}
