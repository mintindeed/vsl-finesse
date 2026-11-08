#include "Internal.h"

#include <algorithm>
#include <map>
#include <set>

namespace vslf
{

void addArticulationMarking (std::vector<ArticulationMarking>& markings, const ArticulationMarking& m)
{
    if (m.end <= m.start || m.keys.empty())
        return;

    std::vector<ArticulationMarking> kept;
    for (const auto& old : markings)
    {
        if (! (old.start < m.end && m.start < old.end))
        {
            kept.push_back (old);
            continue;
        }
        // Keep the parts of the old pick outside the new one.
        if (old.start < m.start)
        {
            auto left = old;
            left.end = m.start;
            kept.push_back (left);
        }
        if (old.end > m.end)
        {
            auto right = old;
            right.start = m.end;
            kept.push_back (right);
        }
    }
    kept.push_back (m);
    std::stable_sort (kept.begin(), kept.end(),
                      [] (const ArticulationMarking& a, const ArticulationMarking& b) { return a.start < b.start; });
    markings = std::move (kept);
}

void clearArticulationMarkings (std::vector<ArticulationMarking>& markings, Tick start, Tick end)
{
    markings.erase (std::remove_if (markings.begin(), markings.end(),
                                    [=] (const ArticulationMarking& m) { return m.start < end && start < m.end; }),
                    markings.end());
}

namespace detail
{

// Rewrites the part's keyswitches for the picked passages. Works on the
// ingested part: notes are monophonic and sorted, keyswitches sorted.
//
// A keyswitch governs the first note at or after it. For every note the app
// works out which key it wants on each level of the articulation tree:
//  - inside a picked passage, the picked keys;
//  - elsewhere, what the file's own keyswitches have selected by then, or the
//    player's start articulation on levels the file never switched.
// Keys are written at a note wherever what the player has received so far
// differs from that. The file's keyswitches governing picked notes are
// dropped; the rest stay exactly as written.
void applyArticulationPicks (Context& ctx, const ArticulationPicks& picks)
{
    auto& part = ctx.part;
    const auto& profile = ctx.profile;
    if (picks.markings.empty() || part.notes.empty())
        return;

    const auto& notes = part.notes;
    const size_t n = notes.size();

    // The pick covering each note's start (the latest-starting one wins).
    std::vector<int> pickOf (n, -1);
    for (size_t i = 0; i < n; ++i)
    {
        Tick latest = 0;
        for (size_t m = 0; m < picks.markings.size(); ++m)
        {
            const auto& pick = picks.markings[m];
            if (pick.keys.empty() || notes[i].start < pick.start || notes[i].start >= pick.end)
                continue;
            if (pickOf[i] < 0 || pick.start >= latest)
            {
                pickOf[i] = (int) m;
                latest = pick.start;
            }
        }
    }

    // The levels the picks switch: only these are switched back afterwards.
    std::set<int> pickedLevels;
    for (const auto& pick : picks.markings)
        for (int key : pick.keys)
            pickedLevels.insert (profile.levelOf (key));

    std::map<int, int> startKey;
    for (int key : picks.startKeys)
        startKey[profile.levelOf (key)] = key;

    // Which note each file keyswitch governs (n: none, it comes after the last note).
    const auto& original = part.keyswitches;
    std::vector<size_t> governs (original.size(), n);
    {
        size_t j = 0;
        for (size_t k = 0; k < original.size(); ++k)
        {
            while (j < n && notes[j].start < original[k].start)
                ++j;
            governs[k] = j;
        }
    }
    auto insidePick = [&] (Tick t) {
        for (const auto& pick : picks.markings)
            if (! pick.keys.empty() && t >= pick.start && t < pick.end)
                return true;
        return false;
    };

    std::vector<Note> out;
    std::map<int, int> fileState; // latest file key per level
    std::map<int, int> sent;      // latest key per level the player has received
    const Tick keyLength = std::max<Tick> (1, part.ppq / 8);
    size_t k = 0;

    for (size_t i = 0; i < n; ++i)
    {
        // The file's keyswitches up to this note.
        for (; k < original.size() && governs[k] <= i; ++k)
        {
            const auto& ks = original[k];
            const int level = profile.levelOf (ks.pitch);
            fileState[level] = ks.pitch;
            if (pickOf[i] >= 0 && governs[k] == i)
            {
                ++ctx.report.keyswitchesReplaced;
                continue;
            }
            out.push_back (ks);
            sent[level] = ks.pitch;
        }

        std::map<int, int> wanted;
        if (pickOf[i] >= 0)
        {
            for (int key : picks.markings[(size_t) pickOf[i]].keys)
                wanted[profile.levelOf (key)] = key;
        }
        else
        {
            for (int level : pickedLevels)
                if (auto f = fileState.find (level); f != fileState.end())
                    wanted[level] = f->second;
                else if (auto s = startKey.find (level); s != startKey.end())
                    wanted[level] = s->second;
        }

        for (const auto& [level, key] : wanted)
        {
            if (auto s = sent.find (level); s != sent.end() && s->second == key)
                continue;
            out.push_back (Note { notes[i].start, notes[i].start + keyLength, key, 100, notes[i].channel });
            sent[level] = key;
            ++ctx.report.keyswitchesWritten;
        }
    }

    // Keyswitches after the last note: kept unless they're inside a pick.
    for (; k < original.size(); ++k)
    {
        if (insidePick (original[k].start))
            ++ctx.report.keyswitchesReplaced;
        else
            out.push_back (original[k]);
    }

    std::stable_sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    part.keyswitches = std::move (out);
}

} // namespace detail
} // namespace vslf
