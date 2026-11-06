// Dynamics: a performance-rule model in the spirit of the KTH rule system
// (phrase arch, high-loud, duration contrast, metric accent), producing one
// level per note plus intra-note swells and phrase-end tapers.

#include "Internal.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace vslp::detail
{

namespace
{

// Phrase arch: rises to the peak, falls more quickly after it.
// Returns roughly -0.35 at the start, +0.4 at the peak, -0.4 at the end.
double archShape (double x, double peak)
{
    peak = std::clamp (peak, 0.25, 1.0);
    double shape;
    if (x <= peak)
        shape = 0.25 + 0.75 * smoothstep (x / peak);
    else
        shape = 1.0 - 0.8 * smoothstep ((x - peak) / std::max (1.0e-6, 1.0 - peak));
    return shape - 0.6;
}

double phraseMedianDuration (const Context& ctx, const Phrase& p)
{
    std::vector<double> d;
    for (size_t i = p.firstNote; i <= p.lastNote; ++i)
        d.push_back (ctx.info[i].duration);
    std::nth_element (d.begin(), d.begin() + long (d.size() / 2), d.end());
    return d[d.size() / 2];
}

} // namespace

void shapeDynamics (Context& ctx)
{
    const auto& s = ctx.settings;
    auto& notes = ctx.part.notes;
    auto& info = ctx.info;
    const double I = std::clamp (s.intensity, 0.0, 2.0);

    for (size_t i = 0; i < notes.size(); ++i)
    {
        const double base = s.useInputVelocity ? notes[i].velocity / 127.0 : s.baseLevel;
        info[i].base = clamp01 (base + s.levelOffset);
    }

    for (const auto& p : ctx.phrases)
    {
        const double length = p.endSeconds - p.startSeconds;
        const double archAmount = 0.28 * I * s.phraseArchAmount * std::clamp (length / 4.0, 0.35, 1.0);
        const double peakPosition = info[p.peakNote].phrasePosition;
        const double medianDur = std::max (0.02, phraseMedianDuration (ctx, p));

        double meanPitch = 0.0;
        for (size_t i = p.firstNote; i <= p.lastNote; ++i)
            meanPitch += notes[i].pitch;
        meanPitch /= double (p.lastNote - p.firstNote + 1);

        const bool singleNote = p.firstNote == p.lastNote;

        for (size_t i = p.firstNote; i <= p.lastNote; ++i)
        {
            auto& in = info[i];
            const auto& art = ctx.articulationOf (i);

            const double arch = singleNote ? 0.0 : archAmount * archShape (in.phrasePosition, peakPosition);
            const double highLoud = std::clamp (0.05 * I * s.highLoudAmount * (notes[i].pitch - meanPitch) / 12.0, -0.08, 0.08);
            const double relDur = std::clamp (std::log2 (std::max (0.01, in.duration) / medianDur), -2.0, 2.0);
            const double durContrast = 0.02 * I * s.durationContrastAmount * relDur;

            in.level = clamp01 (in.base + arch + highLoud + durContrast);

            // Velocity adds the per-note detail the CC curve shouldn't carry.
            const double accent = 0.06 * I * s.metricAccentAmount * (in.metricWeight - 0.35);
            const double human = 0.03 * std::clamp (s.humanizeVelocity, 0.0, 1.0) * noise (s.seed, i, 1);
            in.velocity = clamp01 (in.level + accent + human);

            const int vel = (int) std::lround (in.velocity * 127.0);
            notes[i].velocity = std::clamp (vel, std::max (1, s.velocityMin), std::min (127, s.velocityMax));

            // Long notes swell (except phrase-final ones, which taper).
            const bool crossfades = art.mode != DynamicsMode::Velocity;
            const double longThreshold = 0.7;
            if (crossfades && in.duration >= longThreshold && ! in.phraseEnd)
            {
                const double size = std::clamp ((in.duration - longThreshold) / 2.0 + 0.4, 0.0, 1.0);
                const double variation = 1.0 + 0.25 * noise (s.seed, i, 3);
                in.swell = 0.08 * I * s.longNoteSwellAmount * size * variation;
            }

            if (in.phraseEnd && in.duration >= 0.2)
            {
                const bool lastOfPiece = (i + 1 == notes.size());
                in.taper = 0.18 * I * s.phraseEndTaperAmount * std::clamp (in.duration / 1.0, 0.3, 1.0)
                           * (lastOfPiece ? 1.3 : 1.0);
            }
        }
    }
}

} // namespace vslp::detail
