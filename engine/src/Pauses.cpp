// Human pauses: the small gaps real players can't avoid.
//
//  - Winds and brass breathe where a long phrase was split for breath.
//  - Any instrument lifts briefly where a phrase ends without a written rest
//    (in the spirit of the KTH "punctuation" rule).
//  - String players need a moment when the bow jumps over a string (e.g. a
//    cello going from the C string straight to the A string).
//
// Pauses are taken from the end of the earlier note; the beat never moves.

#include "Internal.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vslp::detail
{

namespace
{

// Picks a string for every note: notes are kept in low positions, and string
// changes are avoided where possible (a Viterbi search over the strings).
void assignStrings (Context& ctx)
{
    const auto& open = ctx.profile.openStrings;
    const auto& notes = ctx.part.notes;
    const size_t n = notes.size();
    const size_t k = open.size();
    if (k == 0 || n == 0)
        return;

    constexpr double impossible = 1.0e9;
    auto noteCost = [&] (int pitch, size_t s) {
        const int above = pitch - open[s];
        if (above < 0)
            return impossible;
        if (above > 24)
            return impossible;
        return std::max (0, above - 7) * 0.5; // higher positions cost more
    };

    std::vector<std::vector<double>> cost (n, std::vector<double> (k, impossible));
    std::vector<std::vector<int>> from (n, std::vector<int> (k, -1));

    for (size_t s = 0; s < k; ++s)
        cost[0][s] = noteCost (notes[0].pitch, s);

    for (size_t i = 1; i < n; ++i)
        for (size_t s = 0; s < k; ++s)
        {
            const double own = noteCost (notes[i].pitch, s);
            if (own >= impossible)
                continue;
            for (size_t p = 0; p < k; ++p)
            {
                if (cost[i - 1][p] >= impossible)
                    continue;
                const double c = cost[i - 1][p] + own + std::abs ((int) s - (int) p) * 1.0;
                if (c < cost[i][s])
                {
                    cost[i][s] = c;
                    from[i][s] = (int) p;
                }
            }
            if (cost[i][s] >= impossible) // previous note was unplayable: start fresh
                cost[i][s] = own;
        }

    // Walk back from the cheapest last string.
    int s = -1;
    double best = impossible;
    for (size_t q = 0; q < k; ++q)
        if (cost[n - 1][q] < best)
        {
            best = cost[n - 1][q];
            s = (int) q;
        }

    for (size_t i = n; i-- > 0;)
    {
        ctx.info[i].string = s;
        if (s < 0)
        {
            // Unplayable here (out of range): pick the cheapest string for the previous note.
            if (i > 0)
            {
                double b = impossible;
                for (size_t q = 0; q < k; ++q)
                    if (cost[i - 1][q] < b)
                    {
                        b = cost[i - 1][q];
                        s = (int) q;
                    }
            }
            continue;
        }
        s = from[i][(size_t) s];
        if (s < 0 && i > 0)
        {
            double b = impossible;
            for (size_t q = 0; q < k; ++q)
                if (cost[i - 1][q] < b)
                {
                    b = cost[i - 1][q];
                    s = (int) q;
                }
        }
    }
}

} // namespace

void findPauses (Context& ctx)
{
    const auto& s = ctx.settings;
    auto& info = ctx.info;
    const size_t n = info.size();

    assignStrings (ctx);

    if (! s.humanPauses)
    {
        for (auto& in : info)
            in.breathAfter = false;
        return;
    }

    constexpr double phraseLiftMs = 60.0;
    constexpr double stringCrossingMs = 45.0;

    for (size_t i = 0; i + 1 < n; ++i)
    {
        auto& in = info[i];
        const bool connected = in.gapAfter < 0.06;

        if (in.breathAfter)
        {
            in.pauseAfterMs = s.breathMs;
            ++ctx.report.breaths;
        }
        else if (in.phraseEnd && in.gapAfter < 0.12)
        {
            in.pauseAfterMs = phraseLiftMs;
            ++ctx.report.pauses;
        }
        else if (connected && in.string >= 0 && info[i + 1].string >= 0
                 && std::abs (in.string - info[i + 1].string) >= 2)
        {
            in.pauseAfterMs = stringCrossingMs;
            ++ctx.report.pauses;
        }
    }
}

} // namespace vslp::detail
