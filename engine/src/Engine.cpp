#include "Internal.h"

#include <algorithm>
#include <map>

namespace vslp
{
namespace detail
{

static uint64_t splitmix (uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

double noise (uint32_t seed, uint64_t a, uint64_t b)
{
    const uint64_t h = splitmix (splitmix (splitmix (seed) ^ a) ^ (b * 0x9E3779B97F4A7C15ull));
    return double (h >> 11) / double (1ull << 53) * 2.0 - 1.0;
}

double clamp01 (double v) { return std::clamp (v, 0.0, 1.0); }

double smoothstep (double x)
{
    x = clamp01 (x);
    return x * x * (3.0 - 2.0 * x);
}

void assignArticulations (Context& ctx)
{
    auto& part = ctx.part;
    const auto& profile = ctx.profile;
    ctx.articulations.clear();
    ctx.articulationKeys.clear();
    ctx.report.keyswitchesIn = (int) part.keyswitches.size();

    std::map<std::vector<int>, int> indexForCombination;
    auto indexFor = [&] (const std::vector<int>& combo) {
        auto it = indexForCombination.find (combo);
        if (it != indexForCombination.end())
            return it->second;
        const int index = (int) ctx.articulations.size();
        ctx.articulations.push_back (profile.resolve (combo));
        ctx.articulationKeys.push_back (combo);
        indexForCombination[combo] = index;
        return index;
    };

    // Current state: the latest key on each level of the articulation tree.
    std::map<int, int> latestKeyForLevel;
    auto currentCombination = [&] {
        std::vector<int> combo;
        for (const auto& [level, pitch] : latestKeyForLevel)
            combo.push_back (pitch);
        std::sort (combo.begin(), combo.end());
        return combo;
    };

    ctx.info.assign (part.notes.size(), NoteInfo {});
    std::map<int, int> notesPerIndex;
    size_t k = 0;
    for (size_t i = 0; i < part.notes.size(); ++i)
    {
        while (k < part.keyswitches.size() && part.keyswitches[k].start <= part.notes[i].start)
        {
            const int pitch = part.keyswitches[k++].pitch;
            latestKeyForLevel[profile.levelOf (pitch)] = pitch;
        }
        const int index = indexFor (currentCombination());
        ctx.info[i].articulation = index;
        ++notesPerIndex[index];
    }

    for (size_t a = 0; a < ctx.articulations.size(); ++a)
    {
        ArticulationUsage u;
        u.keys = ctx.articulationKeys[a];
        u.notes = notesPerIndex[(int) a];
        u.mapped = u.keys.empty() || profile.findArticulation (u.keys) != nullptr;
        u.articulation = ctx.articulations[a].name;
        ctx.report.articulations.push_back (u);

        if (! u.mapped)
        {
            std::string names;
            for (int key : u.keys)
                names += (names.empty() ? "" : " + ") + noteName (key);
            ctx.report.warnings.push_back ("Keyswitch " + names + " isn't mapped in the instrument profile - treating it as '"
                                           + profile.unmappedArticulation.name + "' ("
                                           + toString (profile.unmappedArticulation.mode) + ").");
        }
    }
}

} // namespace detail

Result finesse (const Part& input, const InstrumentProfile& profile, const Settings& settings)
{
    detail::Context ctx { profile, settings, {}, {}, {}, {}, {}, {} };
    ctx.part = ingest (input, profile, settings, ctx.report);

    detail::assignArticulations (ctx);

    if (! ctx.part.notes.empty())
    {
        detail::analyze (ctx);
        detail::shapeDynamics (ctx);
        detail::applyTiming (ctx);
        detail::writeControllers (ctx);
    }
    else
    {
        ctx.report.warnings.push_back ("No notes to perform.");
    }

    Result r;
    r.part = std::move (ctx.part);
    r.phrases = std::move (ctx.phrases);
    r.articulations = std::move (ctx.articulations);
    r.articulationKeys = std::move (ctx.articulationKeys);
    r.articulationIndex.reserve (ctx.info.size());
    for (const auto& i : ctx.info)
        r.articulationIndex.push_back (i.articulation);
    r.report = std::move (ctx.report);
    r.report.notesOut = (int) r.part.notes.size();
    return r;
}

} // namespace vslp
