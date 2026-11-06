#include "vslp/Model.h"

#include <algorithm>

namespace vslp
{

TempoMap::TempoMap (int ppq, std::vector<TempoChange> changes)
    : ppq_ (ppq > 0 ? ppq : 480), changes_ (std::move (changes))
{
    std::stable_sort (changes_.begin(), changes_.end(),
                      [] (const TempoChange& a, const TempoChange& b) { return a.tick < b.tick; });

    // Drop invalid entries and keep only the last change at any given tick.
    std::vector<TempoChange> cleaned;
    for (const auto& c : changes_)
    {
        if (c.microsecondsPerQuarter <= 0.0)
            continue;
        if (! cleaned.empty() && cleaned.back().tick == c.tick)
            cleaned.back() = c;
        else
            cleaned.push_back (c);
    }

    if (cleaned.empty() || cleaned.front().tick > 0)
        cleaned.insert (cleaned.begin(), TempoChange { 0, cleaned.empty() ? 500000.0 : cleaned.front().microsecondsPerQuarter });

    changes_ = std::move (cleaned);

    double seconds = 0.0;
    for (size_t i = 0; i < changes_.size(); ++i)
    {
        const double spt = changes_[i].microsecondsPerQuarter / 1.0e6 / ppq_;
        if (i > 0)
        {
            const auto& prev = segments_.back();
            seconds = prev.seconds + (double (changes_[i].tick) - prev.tick) * prev.secondsPerTick;
        }
        segments_.push_back ({ double (changes_[i].tick), seconds, spt });
    }
}

double TempoMap::secondsAt (double tick) const
{
    auto it = std::upper_bound (segments_.begin(), segments_.end(), tick,
                                [] (double t, const Segment& s) { return t < s.tick; });
    const auto& seg = (it == segments_.begin()) ? segments_.front() : *(it - 1);
    return seg.seconds + (tick - seg.tick) * seg.secondsPerTick;
}

double TempoMap::tickAt (double seconds) const
{
    auto it = std::upper_bound (segments_.begin(), segments_.end(), seconds,
                                [] (double s, const Segment& seg) { return s < seg.seconds; });
    const auto& seg = (it == segments_.begin()) ? segments_.front() : *(it - 1);
    return seg.tick + (seconds - seg.seconds) / seg.secondsPerTick;
}

double TempoMap::bpmAt (double tick) const
{
    auto it = std::upper_bound (segments_.begin(), segments_.end(), tick,
                                [] (double t, const Segment& s) { return t < s.tick; });
    const auto& seg = (it == segments_.begin()) ? segments_.front() : *(it - 1);
    return 60.0 / (seg.secondsPerTick * ppq_);
}

double TempoMap::ticksForMs (double tick, double ms) const
{
    const double s = secondsAt (tick);
    return tickAt (s + ms / 1000.0) - tick;
}

} // namespace vslp
