#pragma once

#include "Model.h"
#include "Profile.h"
#include "Settings.h"

#include <string>
#include <vector>

namespace vslp
{

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
    int controllersRemoved = 0;
    int legatoConnections = 0;
    int keyswitchesIn = 0;
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

// The whole pipeline: ingest -> analyze -> dynamics -> timing -> controller curves.
Result finesse (const Part& input, const InstrumentProfile& profile, const Settings& settings);

} // namespace vslp
