// Crossfaded articulations: the Dimension Controls of a parallel dimension.
//
// Choosing Marcato or XF tremolo is the composer's decision (a keyswitch);
// how much bite or tremolo each note gets is the player's, so the engine
// shapes it:
//  - An added layer (Marcato's staccato attack on top of the sustain) is an
//    accent: one value per note, from the note's place in the music.
//  - A crossfade (sustain into tremolo) is colour: a smooth line that leans
//    into the second sound as the phrase builds to its peak and back after.
// Both centre on the preset's own value for the control and scale with
// Character. Written to whatever the preset maps the control to: the note's
// velocity or a CC.

#include "Internal.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace vslf::detail
{

namespace
{

using Blend = Articulation::Blend;

// 0 at the phrase start, 1 at its peak, back to 0 at the end.
double arch01 (double x, double peak)
{
    peak = std::clamp (peak, 0.25, 0.95);
    x = std::clamp (x, 0.0, 1.0);
    return x <= peak ? smoothstep (x / peak) : 1.0 - smoothstep ((x - peak) / (1.0 - peak));
}

double medianInputVelocity (const Context& ctx, size_t i)
{
    std::vector<int> v;
    const size_t n = ctx.info.size();
    for (size_t j = i >= 3 ? i - 3 : 0; j < std::min (n, i + 4); ++j)
        if (j != i)
            v.push_back (ctx.info[j].inputVelocity);
    if (v.empty())
        return ctx.info[i].inputVelocity;
    std::nth_element (v.begin(), v.begin() + long (v.size() / 2), v.end());
    return v[v.size() / 2];
}

// An accent amount for note i, 0..1.
double accentValue (const Context& ctx, size_t i, double centre, double scale)
{
    const auto& s = ctx.settings;
    const auto& notes = ctx.part.notes;
    const auto& in = ctx.info[i];

    double dev = 0.14 * (in.metricWeight - 0.4); // lean on strong beats

    const double gapBefore = i > 0 ? ctx.info[i - 1].gapAfter : 10.0;
    const bool fresh = in.phraseStart || gapBefore >= 0.25;
    if (fresh)
        dev += 0.10; // a phrase start, or after a rest

    if (i > 0 && ! fresh)
    {
        const int leap = notes[i].pitch - notes[i - 1].pitch;
        if (leap >= 5)
            dev += 0.06; // an upward leap is attacked

        const double sincePrevious = in.onset - ctx.info[i - 1].onset;
        if (sincePrevious < 0.22)
            dev -= 0.14 * (1.0 - sincePrevious / 0.22); // no time for bite in fast notes
        if (leap == 0)
            dev -= 0.04;
    }

    dev += 0.35 * (in.level - 0.6); // louder playing, more bite

    // Notes the file plays louder than their neighbours (notated accents).
    const double accent = (in.inputVelocity - medianInputVelocity (ctx, i)) / 127.0;
    dev += std::clamp (accent * 1.2, -0.12, 0.2);

    dev += 0.04 * std::clamp (s.humanizeVelocity, 0.0, 1.0) * noise (s.seed, i, 11);
    return std::clamp (centre + dev * scale, 0.08, 0.92);
}

// The crossfade at time t, during note i.
double crossfadeValue (const Context& ctx, size_t i, double t, double centre, double scale)
{
    const auto& in = ctx.info[(size_t) i];
    const auto& p = ctx.phrases[(size_t) std::clamp (in.phrase, 0, (int) ctx.phrases.size() - 1)];
    const double length = std::max (1.0e-3, p.endSeconds - p.startSeconds);
    const double peak = ctx.info[p.peakNote].phrasePosition;
    const double shape = arch01 ((t - p.startSeconds) / length, peak);
    const double depth = 0.4 * scale * std::clamp (length / 4.0, 0.35, 1.0); // short phrases barely move
    const double drift = 0.03 * noise (ctx.settings.seed, (uint64_t) in.phrase, 12); // no two phrases alike
    return std::clamp (centre + depth * shape + 0.25 * (in.level - 0.6) * scale + drift, 0.0, 1.0);
}

std::string describe (const Articulation& a)
{
    const auto& b = a.blend;
    std::string text = a.name + ": " + (b.name.empty() ? std::string ("crossfade") : b.name);
    text += b.kind == Blend::Kind::AddedLayer ? " per note" : " follows the phrases";
    text += b.controller < 0 ? " (velocity)" : " (CC" + std::to_string (b.controller) + ")";
    return text;
}

} // namespace

void writeBlends (Context& ctx)
{
    const auto& s = ctx.settings;
    const auto& profile = ctx.profile;
    auto& part = ctx.part;
    auto& notes = part.notes;
    const auto& tempo = part.tempo;
    const size_t n = notes.size();
    if (n == 0 || ctx.phrases.empty())
        return;

    const double scale = std::clamp (s.intensity, 0.0, 2.0) / 0.6; // Character: Expressive = 1
    const double lead = std::max (0.0, s.ccLeadMs) / 1000.0;
    const bool dynamicsWritten = s.presetType != PresetType::Velocity && profile.dynamicsCC >= 0;

    // Would writing this note's blend fight something else?
    auto blocked = [&] (size_t i, const Blend& b) {
        if (b.controller < 0)
            return ctx.velocityIsDynamic (i); // velocity is this note's dynamic
        if (b.controller == profile.expressionCC || (dynamicsWritten && b.controller == profile.dynamicsCC))
            return true; // the engine's own dynamics / expression
        return std::find (ctx.fileControllers.begin(), ctx.fileControllers.end(), b.controller)
               != ctx.fileControllers.end(); // the user's own line wins
    };

    std::map<int, std::vector<ControllerEvent>> lines; // per controller
    std::set<std::string> described;
    auto add = [&] (int cc, double seconds, double value, int channel) {
        auto& line = lines[cc];
        const int v = std::clamp ((int) std::lround (value * 127.0), 0, 127);
        const Tick tick = std::max<Tick> (0, (Tick) std::llround (tempo.tickAt (seconds)));
        if (! line.empty() && line.back().value == v)
            return;
        if (! line.empty() && line.back().tick >= tick)
        {
            line.back().value = v; // the later value wins at the same tick
            return;
        }
        line.push_back ({ tick, cc, v, channel });
    };

    for (size_t i = 0; i < n; ++i)
    {
        const auto& articulation = ctx.articulationOf (i);
        const auto& b = articulation.blend;
        if (b.kind == Blend::Kind::None || blocked (i, b))
            continue;
        described.insert (describe (articulation));
        const auto& in = ctx.info[i];

        if (b.kind == Blend::Kind::AddedLayer)
        {
            const double value = accentValue (ctx, i, b.centre, scale);
            if (b.controller < 0)
            {
                notes[i].velocity = std::clamp ((int) std::lround (value * 127.0), 1, 127);
                continue;
            }
            // Set before this note starts, after the previous one has.
            double at = in.onset - lead;
            if (i > 0)
                at = std::max (at, ctx.info[i - 1].onset + 0.002);
            add (b.controller, at, value, notes[i].channel);
            continue;
        }

        // A crossfade: a line through the note, starting just before it.
        if (b.controller < 0)
        {
            // Velocity can only carry one value per note.
            notes[i].velocity = std::clamp ((int) std::lround (crossfadeValue (ctx, i, in.onset, b.centre, scale) * 127.0), 1, 127);
            continue;
        }
        const double step = std::max (0.005, s.ccIntervalMs / 1000.0) * 2.0;
        double from = in.onset - lead;
        if (i > 0)
            from = std::max (from, ctx.info[i - 1].onset + 0.002);
        for (double t = from; t < in.offset; t += step)
            add (b.controller, t, crossfadeValue (ctx, i, std::max (t, in.onset), b.centre, scale), notes[i].channel);
    }

    for (const auto& [cc, line] : lines)
        part.controllers.insert (part.controllers.end(), line.begin(), line.end());
    std::stable_sort (part.controllers.begin(), part.controllers.end(),
                      [] (const ControllerEvent& a, const ControllerEvent& b) { return a.tick < b.tick; });
    ctx.report.blends.assign (described.begin(), described.end());
}

} // namespace vslf::detail
