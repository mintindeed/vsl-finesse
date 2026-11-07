// Controller curves: CC1 (dynamics / velocity crossfade) and CC11 (expression).
//
// CC1 is one continuous line through the whole part, even across Velocity
// articulations (where it does nothing), so that it already has the right
// value whenever the music switches to a crossfading articulation.
//
// CC11 rests at expressionBase and only dips to taper phrase endings below
// the softest dynamic layer.

#include "Internal.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace vslf::detail
{

namespace
{

struct CurveBuilder
{
    const Context& ctx;
    std::vector<double> knots; // per note: the time its onset value must be reached
    double lead;

    explicit CurveBuilder (const Context& c)
        : ctx (c), lead (std::max (0.0, c.settings.ccLeadMs) / 1000.0)
    {
        for (const auto& in : ctx.info)
            knots.push_back (in.onset - lead);
    }

    // Index of the note whose segment contains t (-1 = before the first knot).
    long segmentAt (double t) const
    {
        auto it = std::upper_bound (knots.begin(), knots.end(), t);
        return long (it - knots.begin()) - 1;
    }

    bool crossfades (size_t i) const { return ctx.crossfades (i); }

    double bump (size_t i, double t) const
    {
        const auto& in = ctx.info[i];
        if (in.swell <= 0.0 || in.duration <= 0.0)
            return 0.0;
        const double u = clamp01 ((t - in.onset) / in.duration);
        const double s = std::sin (u * 3.14159265358979);
        return in.swell * s * s;
    }

    // Fraction (0..1) of the phrase-end taper applied at time t.
    double taperAmount (size_t i, double t) const
    {
        const auto& in = ctx.info[i];
        if (in.duration <= 0.0)
            return t >= in.onset ? 1.0 : 0.0;
        return smoothstep ((t - (in.onset + 0.35 * in.duration)) / (0.65 * in.duration));
    }

    // Value of note i's own shape at time t (ignoring the next note).
    double ownDynamics (size_t i, double t) const
    {
        const auto& in = ctx.info[i];
        double v = in.level;
        if (crossfades (i))
            v += bump (i, t) - in.taper * taperAmount (i, t)
                 + ctx.hairpinAt (std::min (t, in.offset)) - in.hairpin; // hairpins keep moving inside long notes
        return v;
    }

    double ownExpression (size_t i, double t) const
    {
        const auto& s = ctx.settings;
        const auto& in = ctx.info[i];
        const double range = std::max (0, s.expressionBase - s.expressionMin);
        double v = s.expressionBase;

        // Where CC1 can't shape a long note (Velocity presets), CC11 does:
        // the note starts a little below the resting level and grows into it.
        if (! crossfades (i) && in.swell > 0.0 && in.duration > 0.0)
        {
            const double u = clamp01 ((t - in.onset) / in.duration);
            const double s2 = std::sin (u * 3.14159265358979);
            v -= std::min (1.0, in.swell / 0.08) * 0.35 * range * (1.0 - s2 * s2);
        }

        // Phrase-end taper: up to half the base-to-min range at full intensity.
        if (in.phraseEnd && in.taper > 0.0)
            v -= std::min (1.0, in.taper / 0.18) * 0.5 * range * taperAmount (i, t);

        return v;
    }

    // Generic segment evaluation shared by both curves.
    template <typename Own, typename Start>
    double evaluate (double t, Own own, Start startValue) const
    {
        const long seg = segmentAt (t);
        const size_t n = knots.size();
        if (seg < 0)
            return startValue (0);

        const size_t i = (size_t) seg;
        if (i + 1 >= n)
            return own (i, t);

        const size_t j = i + 1;
        const double k0 = knots[i];
        const double k1 = knots[j];
        const double span = std::max (1.0e-6, k1 - k0);

        if (! ctx.info[i].phraseEnd)
        {
            // Within a phrase: glide from this note's value to the next.
            const double w = smoothstep ((t - k0) / span);
            const double from = own (i, t);
            return from + (startValue (j) - startValue (i)) * w;
        }

        // Across a phrase boundary: keep this note's shape (and its taper)
        // through the release, then move to the next phrase's start value just
        // before it begins.
        const double ramp = std::min (0.06, span * 0.3);
        const double rampStart = k1 - ramp;
        if (t < rampStart)
            return own (i, t);
        const double from = own (i, rampStart);
        return from + (startValue (j) - from) * clamp01 ((t - rampStart) / ramp);
    }

    double dynamicsAt (double t) const
    {
        return evaluate (t,
                         [this] (size_t i, double tt) { return ownDynamics (i, tt); },
                         [this] (size_t i) { return ctx.info[i].level; });
    }

    double expressionAt (double t) const
    {
        return evaluate (t,
                         [this] (size_t i, double tt) { return ownExpression (i, tt); },
                         [this] (size_t) { return double (ctx.settings.expressionBase); });
    }
};

int dominantChannel (const Part& part)
{
    std::map<int, int> count;
    for (const auto& n : part.notes)
        ++count[n.channel];
    int best = 1, bestCount = -1;
    for (const auto& [ch, c] : count)
        if (c > bestCount)
        {
            best = ch;
            bestCount = c;
        }
    return best;
}

} // namespace

void writeControllers (Context& ctx)
{
    const auto& s = ctx.settings;
    const auto& profile = ctx.profile;
    auto& part = ctx.part;
    const auto& tempo = part.tempo;

    if (ctx.info.empty())
        return;

    CurveBuilder curve (ctx);
    const bool writeDynamics = s.presetType != PresetType::Velocity && profile.dynamicsCC >= 0;
    const int channel = dominantChannel (part);

    // Sample times: a regular grid plus every knot, so onset values are exact.
    const double step = std::max (0.002, s.ccIntervalMs / 1000.0);
    const double t0 = std::max (tempo.secondsAt (0.0), curve.knots.front());
    const double t1 = ctx.info.back().offset + 0.5;

    std::vector<double> times;
    for (double t = t0; t <= t1; t += step)
        times.push_back (t);
    for (double k : curve.knots)
        times.push_back (std::max (t0, k));
    std::sort (times.begin(), times.end());
    times.erase (std::unique (times.begin(), times.end(), [] (double a, double b) { return std::abs (a - b) < 1.0e-6; }),
                 times.end());

    std::vector<ControllerEvent> out;
    int lastDyn = -1, lastExp = -1;

    for (double t : times)
    {
        const Tick tick = std::max<Tick> (0, (Tick) std::llround (tempo.tickAt (t)));

        // Velocity presets ignore CC1, so none is written.
        const int dyn = std::clamp ((int) std::lround (curve.dynamicsAt (t) * 127.0), s.dynamicsMin, s.dynamicsMax);
        if (writeDynamics && dyn != lastDyn)
        {
            out.push_back ({ tick, profile.dynamicsCC, dyn, channel });
            lastDyn = dyn;
        }

        if (profile.expressionCC >= 0)
        {
            const int exp = std::clamp ((int) std::lround (curve.expressionAt (t)), s.expressionMin, 127);
            if (exp != lastExp)
            {
                out.push_back ({ tick, profile.expressionCC, exp, channel });
                lastExp = exp;
            }
        }
    }

    // Collapse several values for the same controller at the same tick (keep the last).
    std::vector<ControllerEvent> collapsed;
    for (const auto& e : out)
    {
        if (! collapsed.empty())
        {
            auto it = std::find_if (collapsed.rbegin(), collapsed.rend(),
                                    [&e] (const ControllerEvent& c) { return c.controller == e.controller; });
            if (it != collapsed.rend() && it->tick == e.tick)
            {
                it->value = e.value;
                continue;
            }
        }
        collapsed.push_back (e);
    }

    // Controllers written here replace what the file had for them.
    std::vector<int> written;
    for (const auto& e : collapsed)
        if (std::find (written.begin(), written.end(), e.controller) == written.end())
            written.push_back (e.controller);
    const auto before = part.controllers.size();
    part.controllers.erase (std::remove_if (part.controllers.begin(), part.controllers.end(),
                                            [&] (const ControllerEvent& c) {
                                                return std::find (written.begin(), written.end(), c.controller) != written.end();
                                            }),
                            part.controllers.end());
    ctx.report.controllersRemoved += (int) (before - part.controllers.size());

    part.controllers.insert (part.controllers.end(), collapsed.begin(), collapsed.end());
    std::stable_sort (part.controllers.begin(), part.controllers.end(),
                      [] (const ControllerEvent& a, const ControllerEvent& b) { return a.tick < b.tick; });
}

} // namespace vslf::detail
