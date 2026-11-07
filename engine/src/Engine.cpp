#include "Internal.h"

#include <algorithm>
#include <map>

namespace vslf
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

bool Context::markedLevel (double t, double& level) const
{
    // The most recently started Dynamic marking covering t wins.
    bool found = false;
    double latestStart = -1.0e18;
    for (const auto& m : markings)
        if (m.kind == Marking::Kind::Dynamic && t >= m.start && t < m.end && m.start >= latestStart)
        {
            latestStart = m.start;
            level = dynamicValue (m.level);
            found = true;
        }
    return found;
}

double Context::hairpinAt (double t) const
{
    // A hairpin grows two dynamic steps over its passage and keeps that
    // arrival level until the next Dynamic marking starts.
    constexpr double steps = 0.2;
    double offset = 0.0;
    for (const auto& h : markings)
    {
        if (h.kind == Marking::Kind::Dynamic || t < h.start)
            continue;

        bool reset = false;
        for (const auto& d : markings)
            if (d.kind == Marking::Kind::Dynamic && d.start > h.start && d.start <= t)
                reset = true;
        if (reset)
            continue;

        const double length = std::max (1.0e-6, h.end - h.start);
        const double progress = smoothstep ((t - h.start) / length);
        offset += (h.kind == Marking::Kind::Crescendo ? steps : -steps) * progress;
    }
    return offset;
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
    }
}

} // namespace detail

void addMarking (std::vector<Marking>& markings, const Marking& m)
{
    if (m.end <= m.start)
        return;

    const bool isDynamic = m.kind == Marking::Kind::Dynamic;
    std::vector<Marking> kept;
    for (const auto& old : markings)
    {
        const bool sameFamily = (old.kind == Marking::Kind::Dynamic) == isDynamic;
        const bool overlaps = old.start < m.end && m.start < old.end;
        if (! sameFamily || ! overlaps)
        {
            kept.push_back (old);
            continue;
        }
        if (! isDynamic)
            continue; // hairpins are replaced whole

        // Keep the parts of the old dynamic outside the new one.
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
    std::stable_sort (kept.begin(), kept.end(), [] (const Marking& a, const Marking& b) { return a.start < b.start; });
    markings = std::move (kept);
}

void clearMarkings (std::vector<Marking>& markings, Tick start, Tick end)
{
    markings.erase (std::remove_if (markings.begin(), markings.end(),
                                    [=] (const Marking& m) { return m.start < end && start < m.end; }),
                    markings.end());
}

const char* dynamicSymbol (int level)
{
    static const char* symbols[] = { "pp", "p", "mp", "mf", "f", "ff" };
    return symbols[std::clamp (level, 0, numDynamicLevels - 1)];
}

const char* dynamicMeaning (int level)
{
    static const char* meanings[] = { "pianissimo, very soft", "piano, soft", "mezzo-piano, moderately soft",
                                      "mezzo-forte, moderately loud", "forte, loud", "fortissimo, very loud" };
    return meanings[std::clamp (level, 0, numDynamicLevels - 1)];
}

double dynamicValue (int level)
{
    static const double values[] = { 0.24, 0.36, 0.48, 0.6, 0.74, 0.88 };
    return values[std::clamp (level, 0, numDynamicLevels - 1)];
}

Result finesse (const Part& input, const InstrumentProfile& profile, const Settings& settings,
                const std::vector<Marking>& markings)
{
    detail::Context ctx { profile, settings, {}, {}, {}, {}, {}, {}, {} };
    ctx.part = ingest (input, profile, settings, ctx.report);

    for (const auto& m : markings)
        if (m.end > m.start)
            ctx.markings.push_back ({ m.kind, input.tempo.secondsAt ((double) m.start),
                                      input.tempo.secondsAt ((double) m.end), m.level });

    detail::assignArticulations (ctx);

    if (! ctx.part.notes.empty())
    {
        detail::analyze (ctx);
        detail::findPauses (ctx);
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

} // namespace vslf
