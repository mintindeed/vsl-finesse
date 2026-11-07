#pragma once

#include "Model.h"
#include "Profile.h"
#include "Settings.h"

#include <string>
#include <vector>

namespace vslf
{

// A dynamic marking the user puts on a passage, like in a score.
//  - Dynamic: pp..ff for the passage (level 0 = pp ... 5 = ff).
//  - Crescendo / Diminuendo: grow two dynamic steps louder / softer across
//    the passage, and stay there until the next dynamic marking.
struct Marking
{
    enum class Kind
    {
        Dynamic,
        Crescendo,
        Diminuendo
    };

    Kind kind = Kind::Dynamic;
    Tick start = 0;
    Tick end = 0;
    int level = 3; // Dynamic only: 0 pp, 1 p, 2 mp, 3 mf, 4 f, 5 ff
};

// Adds a marking, replacing what was there: a dynamic replaces the dynamics
// it overlaps (trimming or splitting them), a crescendo/diminuendo replaces
// the hairpins it overlaps.
void addMarking (std::vector<Marking>& markings, const Marking& marking);

// Removes markings overlapping [start, end).
void clearMarkings (std::vector<Marking>& markings, Tick start, Tick end);

// An articulation the user picks for a passage, e.g. "Long notes > Marcato".
// keys are the keyswitches that select it in the player, one per level of its
// articulation tree (MIDI note numbers, e.g. 25 and 37 for C#0 + C#1 in
// Logic's naming).
struct ArticulationMarking
{
    Tick start = 0;
    Tick end = 0;
    std::vector<int> keys;
    std::string name;
};

// What the user picked, and the articulation the player is in before any
// keyswitch (its keys, if known), so the part can switch back to it after a
// picked passage.
struct ArticulationPicks
{
    std::vector<ArticulationMarking> markings;
    std::vector<int> startKeys;
};

// Adds a picked articulation, replacing the parts of older picks it covers
// (trimming or splitting them).
void addArticulationMarking (std::vector<ArticulationMarking>& markings, const ArticulationMarking& marking);

// Removes picked articulations overlapping [start, end).
void clearArticulationMarkings (std::vector<ArticulationMarking>& markings, Tick start, Tick end);

constexpr int numDynamicLevels = 6;
const char* dynamicSymbol (int level);   // "pp"
const char* dynamicMeaning (int level);  // "pianissimo, very soft"
double dynamicValue (int level);         // 0..1 (x127 = CC1 / velocity)

struct Phrase
{
    size_t firstNote = 0; // indices into Result::part.notes
    size_t lastNote = 0;
    size_t peakNote = 0;
    double startSeconds = 0.0;
    double endSeconds = 0.0;
};

// A key combination found in the file and how many notes it governs.
struct ArticulationUsage
{
    std::vector<int> keys; // empty = before any keyswitch
    int notes = 0;
    bool mapped = false;
    std::string articulation;
};

struct Report
{
    int notesIn = 0;
    int notesOut = 0;
    int chordNotesRemoved = 0;
    int overlapsTrimmed = 0;
    int controllersRemoved = 0;  // the file's controller events not in the output
    int controllersKept = 0;     // the file's controller events kept
    int legatoConnections = 0;
    int breaths = 0;     // wind/brass breaths added
    int pauses = 0;      // other human pauses (phrase ends, string crossings)
    int keyswitchesIn = 0;
    // Crossfaded articulations shaped, e.g. "Long notes > Marcato: Attack per note (velocity)".
    std::vector<std::string> blends;
    int keyswitchesWritten = 0; // for picked articulations
    int keyswitchesReplaced = 0; // file keyswitches inside picked passages
    std::vector<ArticulationUsage> articulations;
    std::vector<std::string> warnings;
};

struct Result
{
    Part part;                  // the finessed part
    std::vector<Phrase> phrases;
    std::vector<int> articulationIndex; // per output note: index into `articulations`
    std::vector<Articulation> articulations;
    std::vector<std::vector<int>> articulationKeys; // key combination for each entry of `articulations`
    Report report;
};

// Step 1 only: separate keyswitches, wipe CCs, remove overlaps. Exposed for
// the app (to show what the engine sees) and for tests.
Part ingest (const Part& input, const InstrumentProfile& profile, const Settings& settings, Report& report);

// The whole pipeline: ingest -> picked articulations -> analyze -> dynamics
// -> timing -> controller curves. Markings and picks use the input part's ticks.
//
// Picked articulations are the one place keyswitches are written: each picked
// passage starts with its keys (where the player isn't in it already), the
// file's own keyswitches inside it are replaced, and after it the part
// switches back to what the file (or, before any keyswitch, the player's
// start articulation) had. Without picks, keyswitches are kept as written.
Result finesse (const Part& input, const InstrumentProfile& profile, const Settings& settings,
                const std::vector<Marking>& markings = {}, const ArticulationPicks& picks = {});

} // namespace vslf
