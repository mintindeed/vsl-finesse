#pragma once

#include <vslp/Model.h>

#include <algorithm>
#include <cmath>

// Bar numbers (1-based) from a part's time signatures.
inline int barAt (const vslp::Part& part, vslp::Tick tick)
{
    auto meters = part.timeSignatures;
    if (meters.empty() || meters.front().tick > 0)
        meters.insert (meters.begin(), { 0, 4, 4 });

    int bar = 1;
    for (size_t i = 0; i < meters.size(); ++i)
    {
        const auto& m = meters[i];
        const double barTicks = part.ppq * 4.0 * m.numerator / std::max (1, m.denominator);
        const double until = i + 1 < meters.size() ? (double) meters[i + 1].tick : 1.0e18;
        if ((double) tick < until)
            return bar + (int) std::floor (((double) tick - (double) m.tick) / barTicks);
        bar += (int) std::ceil ((until - (double) m.tick) / barTicks);
    }
    return bar;
}

// Rounds a tick to the nearest beat (quarter note).
inline vslp::Tick snapToBeat (const vslp::Part& part, double tick)
{
    const double beat = part.ppq;
    return (vslp::Tick) std::max (0.0, std::round (tick / beat) * beat);
}
