// Analysis: timing in seconds, metric weight, phrase boundaries and peaks.

#include "Internal.h"

#include <algorithm>
#include <cmath>

namespace vslf::detail
{

namespace
{

TimeSignature activeTimeSignature (const Part& part, Tick t)
{
    TimeSignature ts { 0, 4, 4 };
    for (const auto& s : part.timeSignatures)
        if (s.tick <= t)
            ts = s;
    return ts;
}

bool onGrid (Tick pos, Tick grid, Tick tolerance)
{
    if (grid <= 0)
        return false;
    const Tick r = pos % grid;
    return r <= tolerance || grid - r <= tolerance;
}

double metricWeight (const Part& part, Tick t)
{
    const auto ts = activeTimeSignature (part, t);
    const int num = std::max (1, ts.numerator);
    const int den = std::max (1, ts.denominator);
    const Tick unit = Tick (part.ppq) * 4 / den;
    const bool compound = den >= 8 && num % 3 == 0 && num > 3;
    const Tick beat = compound ? unit * 3 : unit;
    const Tick bar = unit * num;
    const Tick tol = std::max<Tick> (1, part.ppq / 48);

    if (bar <= 0 || t < ts.tick)
        return 0.0;

    const Tick pos = (t - ts.tick) % bar;
    if (pos <= tol || bar - pos <= tol)
        return 1.0;

    if (onGrid (pos, beat, tol))
    {
        const int beatsPerBar = int (bar / beat);
        const int index = int ((pos + tol) / beat);
        if (beatsPerBar >= 4 && beatsPerBar % 2 == 0 && index == beatsPerBar / 2)
            return 0.6;
        return 0.45;
    }

    if (onGrid (pos, compound ? unit : beat / 2, tol))
        return 0.2;

    return 0.0;
}

double median (std::vector<double> v)
{
    if (v.empty())
        return 0.0;
    std::nth_element (v.begin(), v.begin() + long (v.size() / 2), v.end());
    return v[v.size() / 2];
}

// A place where a phrase could end (after note `index`), with a strength.
struct Candidate
{
    size_t index;
    double strength;
};

} // namespace

static void splitLongSegments (Context& ctx, std::vector<size_t>& boundaries, double maxSeconds, bool breath)
{
    // boundaries holds indices of phrase-final notes, sorted, always including the last note.
    auto& info = ctx.info;
    const auto& notes = ctx.part.notes;

    bool changed = true;
    while (changed)
    {
        changed = false;
        size_t first = 0;
        for (size_t b = 0; b < boundaries.size(); ++b)
        {
            const size_t last = boundaries[b];
            const double length = info[last].offset - info[first].onset;
            if (length > maxSeconds && last > first + 2)
            {
                // Choose the best split point: long notes, leaps and short gaps,
                // weighted towards the middle of the segment.
                size_t best = first;
                double bestScore = -1.0;
                for (size_t i = first + 1; i + 1 < last; ++i)
                {
                    const double mid = (info[i].offset - info[first].onset) / length;
                    const double centre = 1.0 - std::abs (mid - 0.5) * 1.6;
                    const double leap = std::min (1.0, std::abs (notes[i + 1].pitch - notes[i].pitch) / 12.0);
                    const double lengthScore = std::min (1.0, info[i].duration / std::max (0.05, info[i].beatSeconds * 2.0));
                    const double gapScore = std::min (1.0, info[i].gapAfter / 0.2);
                    const double nextIsStrong = info[i + 1].metricWeight;
                    const double score = centre * 0.4 + lengthScore * 0.25 + gapScore * 0.15 + leap * 0.1 + nextIsStrong * 0.1;
                    if (score > bestScore)
                    {
                        bestScore = score;
                        best = i;
                    }
                }
                if (best > first)
                {
                    boundaries.insert (boundaries.begin() + long (b), best);
                    if (breath)
                        info[best].breathAfter = true;
                    changed = true;
                    break;
                }
            }
            first = last + 1;
        }
    }
}

void analyze (Context& ctx)
{
    auto& part = ctx.part;
    auto& info = ctx.info;
    const auto& tempo = part.tempo;
    const size_t n = part.notes.size();

    for (size_t i = 0; i < n; ++i)
    {
        auto& in = info[i];
        const auto& note = part.notes[i];
        in.onset = tempo.secondsAt (double (note.start));
        in.offset = tempo.secondsAt (double (note.end));
        in.duration = std::max (0.0, in.offset - in.onset);
        in.beatSeconds = 60.0 / tempo.bpmAt (double (note.start));
        in.metricWeight = metricWeight (part, note.start);
    }

    for (size_t i = 0; i < n; ++i)
    {
        if (i + 1 < n)
        {
            info[i].ioi = info[i + 1].onset - info[i].onset;
            info[i].gapAfter = std::max (0.0, info[i + 1].onset - info[i].offset);
        }
        else
        {
            info[i].ioi = 0.0;
            info[i].gapAfter = 1.0e9;
        }
    }

    // --- Phrase boundaries ---------------------------------------------------
    // 1. Rests end phrases.
    std::vector<size_t> boundaries;
    for (size_t i = 0; i + 1 < n; ++i)
    {
        const double restThreshold = std::max (0.3, 0.75 * info[i].beatSeconds);
        if (info[i].gapAfter >= restThreshold)
            boundaries.push_back (i);
    }
    boundaries.push_back (n - 1);

    // 2. A long note followed by shorter ones also closes a phrase, if both
    //    halves stay reasonably long.
    {
        std::vector<size_t> withLongNotes;
        size_t first = 0;
        for (size_t b : boundaries)
        {
            size_t segStart = first;
            for (size_t i = first + 2; i + 3 <= b; ++i)
            {
                std::vector<double> recent;
                for (size_t k = (i >= segStart + 4 ? i - 4 : segStart); k < i; ++k)
                    recent.push_back (info[k].duration);
                const double typical = median (recent);
                const bool isLong = info[i].duration >= std::max (1.5 * info[i].beatSeconds, 2.0 * typical);
                const bool nextShorter = info[i + 1].duration < info[i].duration * 0.6;
                const double before = info[i].offset - info[segStart].onset;
                const double after = info[b].offset - info[i + 1].onset;
                if (isLong && nextShorter && before >= 2.0 && after >= 2.0)
                {
                    withLongNotes.push_back (i);
                    segStart = i + 1;
                }
            }
            withLongNotes.push_back (b);
            first = b + 1;
        }
        boundaries = std::move (withLongNotes);
    }

    // 3. Breath limits for winds and brass, then a general upper limit.
    if (ctx.profile.maxBreathSeconds > 0.0)
        splitLongSegments (ctx, boundaries, ctx.profile.maxBreathSeconds, true);
    splitLongSegments (ctx, boundaries, 12.0, false);

    // --- Build phrases --------------------------------------------------------
    ctx.phrases.clear();
    size_t first = 0;
    for (size_t b : boundaries)
    {
        Phrase p;
        p.firstNote = first;
        p.lastNote = b;
        p.startSeconds = info[first].onset;
        p.endSeconds = info[b].offset;

        const int index = (int) ctx.phrases.size();
        const double length = std::max (1.0e-6, p.endSeconds - p.startSeconds);

        int lo = 127, hi = 0;
        double longest = 0.0;
        for (size_t i = first; i <= b; ++i)
        {
            lo = std::min (lo, part.notes[i].pitch);
            hi = std::max (hi, part.notes[i].pitch);
            longest = std::max (longest, info[i].duration);
        }

        double bestScore = -1.0;
        p.peakNote = first;
        for (size_t i = first; i <= b; ++i)
        {
            auto& in = info[i];
            in.phrase = index;
            in.phraseStart = (i == first);
            in.phraseEnd = (i == b);
            in.phrasePosition = (in.onset - p.startSeconds) / length;

            const double pitchScore = hi > lo ? double (part.notes[i].pitch - lo) / double (hi - lo) : 0.5;
            const double durScore = longest > 0.0 ? in.duration / longest : 0.0;
            const double prior = std::clamp (1.0 - std::abs (in.phrasePosition - 0.6) * 1.2, 0.0, 1.0);
            const double score = 0.5 * pitchScore + 0.3 * durScore + 0.2 * prior + 0.1 * in.metricWeight;
            if (score > bestScore)
            {
                bestScore = score;
                p.peakNote = i;
            }
        }

        ctx.phrases.push_back (p);
        first = b + 1;
    }
}

} // namespace vslf::detail
