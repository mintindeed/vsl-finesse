// Timing: humanized onsets, per-articulation attack offsets, legato overlaps,
// clean repeated notes, breaths - while keeping keyswitches ahead of the notes
// they switch.

#include "Internal.h"

#include <algorithm>
#include <cmath>

namespace vslp::detail
{

void applyTiming (Context& ctx)
{
    const auto& s = ctx.settings;
    auto& part = ctx.part;
    auto& notes = part.notes;
    auto& info = ctx.info;
    const auto& tempo = part.tempo;
    const size_t n = notes.size();

    const auto ticks = [&tempo] (Tick at, double ms) {
        return (Tick) std::llround (tempo.ticksForMs (double (at), ms));
    };

    const std::vector<Note> original = notes;

    // 1. Onsets: humanize + attack compensation, keeping note order.
    for (size_t i = 0; i < n; ++i)
    {
        const auto& art = ctx.articulationOf (i);
        const double shiftMs = art.attackOffsetMs + std::max (0.0, s.humanizeTimingMs) * noise (s.seed, i, 2);
        Tick start = original[i].start + ticks (original[i].start, shiftMs);
        start = std::max<Tick> (0, start);
        if (i > 0)
            start = std::max (start, notes[i - 1].start + 1);
        const Tick delta = start - original[i].start;
        notes[i].start = start;
        notes[i].end = std::max (start + 1, original[i].end + delta);
    }

    // 2. Note ends.
    for (size_t i = 0; i + 1 < n; ++i)
    {
        auto& note = notes[i];
        const auto& next = notes[i + 1];
        const auto& art = ctx.articulationOf (i);
        const auto& nextArt = ctx.articulationOf (i + 1);
        const Tick minLength = std::max<Tick> (1, ticks (note.start, s.minNoteMs));

        // Same-pitch notes can't overlap (the note-off would cut the new note),
        // so repeated pitches are re-articulated even in legato.
        const bool connect = art.legato && nextArt.legato && ! info[i].breathAfter
                             && next.pitch != note.pitch
                             && info[i].gapAfter * 1000.0 <= s.legatoMaxGapMs;
        if (connect)
        {
            Tick overlap = ticks (next.start, s.legatoOverlapMs);
            overlap = std::min (overlap, (next.end - next.start) / 2);
            note.end = std::max (note.start + minLength, next.start + std::max<Tick> (1, overlap));
            ++ctx.report.legatoConnections;
            continue;
        }

        // Monophonic: never overlap the next note.
        Tick latestEnd = next.start;

        // Repeated pitches need a small gap to re-articulate cleanly.
        if (next.pitch == note.pitch)
            latestEnd = next.start - ticks (next.start, s.repeatedNoteGapMs);

        // A breath before the next phrase (winds and brass).
        if (info[i].breathAfter)
        {
            const Tick breath = ticks (note.start, s.breathMs);
            const Tick keep = Tick (double (original[i].end - original[i].start) * 0.6);
            latestEnd = std::min (latestEnd, std::max (note.start + keep, next.start - breath));
        }

        note.end = std::min (note.end, latestEnd);
        note.end = std::max (note.end, std::min (note.start + minLength, next.start - 1));
        note.end = std::max (note.end, note.start + 1);
    }

    // 3. Keyswitches must still arrive before the note they switch.
    //    A keyswitch governs the first note whose original start is at or after it.
    if (! part.keyswitches.empty() && n > 0)
    {
        size_t j = 0;
        for (auto& ks : part.keyswitches)
        {
            while (j < n && original[j].start < ks.start)
                ++j;
            if (j >= n)
                break;

            const Tick lead = std::max<Tick> (1, ticks (notes[j].start, 2.0));
            if (notes[j].start - lead < ks.start)
            {
                Tick moved = notes[j].start - lead;
                if (j > 0)
                    moved = std::max (moved, notes[j - 1].start + 1);
                moved = std::max<Tick> (0, moved);
                const Tick delta = moved - ks.start;
                ks.start += delta;
                ks.end += delta;
            }
        }
    }

    // 4. Refresh the analysis times for the controller writer.
    for (size_t i = 0; i < n; ++i)
    {
        info[i].onset = tempo.secondsAt (double (notes[i].start));
        info[i].offset = tempo.secondsAt (double (notes[i].end));
        info[i].duration = std::max (0.0, info[i].offset - info[i].onset);
    }
}

} // namespace vslp::detail
