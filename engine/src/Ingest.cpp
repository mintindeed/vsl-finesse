#include "Internal.h"

#include <algorithm>

namespace vslf
{

Part ingest (const Part& input, const InstrumentProfile& profile, const Settings& settings, Report& report)
{
    Part out = input;
    out.notes.clear();
    out.keyswitches = input.keyswitches;
    out.controllers.clear();

    report.notesIn = (int) input.notes.size();

    // 1. Keyswitches are kept exactly as written; everything else is a performance note.
    for (const auto& n : input.notes)
    {
        if (n.length() <= 0)
            continue;
        if (profile.isKeyswitch (n.pitch))
            out.keyswitches.push_back (n);
        else
            out.notes.push_back (n);
    }

    std::stable_sort (out.keyswitches.begin(), out.keyswitches.end(),
                      [] (const Note& a, const Note& b) { return a.start < b.start; });

    // 2. Existing CCs are wiped (the engine writes its own), except preserved ones.
    for (const auto& c : input.controllers)
    {
        if (profile.preservesController (c.controller))
            out.controllers.push_back (c);
        else
            ++report.controllersRemoved;
    }

    // 3. One instrument per file: notes sounding at the same time are mistakes.
    //    Chords keep one note; overlapping notes are trimmed.
    const bool keepHighest = settings.chords == ChordResolution::KeepHighest;
    std::stable_sort (out.notes.begin(), out.notes.end(), [keepHighest] (const Note& a, const Note& b) {
        if (a.start != b.start)
            return a.start < b.start;
        return keepHighest ? a.pitch > b.pitch : a.pitch < b.pitch;
    });

    const Tick chordTolerance = std::max<Tick> (1, input.ppq / 96); // ~5 ticks at 480 ppq

    std::vector<Note> mono;
    mono.reserve (out.notes.size());
    for (size_t i = 0; i < out.notes.size();)
    {
        // Gather notes starting (almost) together.
        size_t j = i + 1;
        while (j < out.notes.size() && out.notes[j].start - out.notes[i].start <= chordTolerance)
            ++j;

        size_t best = i;
        for (size_t k = i + 1; k < j; ++k)
        {
            const bool better = keepHighest ? out.notes[k].pitch > out.notes[best].pitch
                                            : out.notes[k].pitch < out.notes[best].pitch;
            if (better)
                best = k;
        }

        Note kept = out.notes[best];
        kept.start = out.notes[i].start;
        mono.push_back (kept);
        report.chordNotesRemoved += (int) (j - i - 1);
        i = j;
    }

    for (size_t i = 0; i + 1 < mono.size(); ++i)
    {
        if (mono[i].end > mono[i + 1].start)
        {
            mono[i].end = mono[i + 1].start;
            ++report.overlapsTrimmed;
        }
    }

    mono.erase (std::remove_if (mono.begin(), mono.end(), [] (const Note& n) { return n.length() <= 0; }), mono.end());
    out.notes = std::move (mono);

    if (report.chordNotesRemoved > 0)
        report.warnings.push_back (std::to_string (report.chordNotesRemoved)
                                   + " chord note(s) removed - the file should contain one melodic line.");

    return out;
}

} // namespace vslf
