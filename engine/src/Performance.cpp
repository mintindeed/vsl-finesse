// The player's performance controls: Attack, Release and Legato blur.
//
// Together they're how a note starts, ends and joins the next one, which a
// player varies without thinking about it:
//  - Release: a note left to ring into the room at a phrase end or before a
//    rest, cut shorter in detached and fast playing.
//  - Attack: a softer onset for quiet entries (the pp entrance "out of
//    nothing"); the preset's own attack everywhere else.
//  - Legato blur: smoother joins in slow, soft legato, cleaner ones in fast
//    runs. Only written for connected notes, where it does something.
// Each is centred on the preset's own value, set just before the note it
// applies to (never before the previous note has started), and scaled by
// Character. Nothing is written where the preset doesn't offer the control,
// or where its controller already carries something else.

#include "Internal.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace vslf::detail
{

void writePerformControls (Context& ctx)
{
    const auto& s = ctx.settings;
    const auto& profile = ctx.profile;
    auto& part = ctx.part;
    const auto& tempo = part.tempo;
    const auto& info = ctx.info;
    const size_t n = part.notes.size();
    if (n == 0)
        return;

    const double scale = std::clamp (s.intensity, 0.0, 2.0) / 0.6; // Character: Expressive = 1
    const double lead = std::max (0.0, s.ccLeadMs) / 1000.0;

    // Controllers already carrying something: the file's kept ones, the
    // engine's dynamics and expression, and the crossfaded articulations.
    std::vector<int> taken = ctx.fileControllers;
    taken.push_back (profile.dynamicsCC);
    taken.push_back (profile.expressionCC);
    for (const auto& a : ctx.articulations)
        if (a.blend.kind != Articulation::Blend::Kind::None)
            taken.push_back (a.blend.controller);
    auto usable = [&] (const InstrumentProfile::PerformControl& c) {
        return c.controller >= 0 && c.controller <= 127
               && std::find (taken.begin(), taken.end(), c.controller) == taken.end();
    };

    std::vector<ControllerEvent> out;
    std::vector<std::string> shaped;
    auto write = [&] (const InstrumentProfile::PerformControl& c, size_t i, double value) {
        double at = info[i].onset - lead;
        if (i > 0)
            at = std::max (at, info[i - 1].onset + 0.002);
        const int v = std::clamp ((int) std::lround (std::clamp (value, 0.0, 1.0) * 127.0), 0, 127);
        for (auto it = out.rbegin(); it != out.rend(); ++it)
            if (it->controller == c.controller)
            {
                if (it->value == v)
                    return; // unchanged
                break;
            }
        out.push_back ({ std::max<Tick> (0, (Tick) std::llround (tempo.tickAt (at))), c.controller, v, part.notes[i].channel });
    };

    auto gapBefore = [&] (size_t i) { return i > 0 ? info[i - 1].gapAfter : 10.0; };
    auto connected = [&] (size_t i) { return i > 0 && gapBefore (i) * 1000.0 <= s.legatoMaxGapMs; };

    // Release: every note gets one, read when it ends.
    if (const auto& c = profile.releaseControl; usable (c))
    {
        shaped.push_back ("Release (CC" + std::to_string (c.controller) + ")");
        for (size_t i = 0; i < n; ++i)
        {
            const auto& in = info[i];
            double dev = 0.0;
            if (i + 1 == n)
                dev += 0.30; // the end of the piece rings longest
            else if (in.phraseEnd || in.gapAfter >= 0.3)
                dev += 0.12 + 0.12 * std::clamp ((in.gapAfter - 0.3) / 1.5, 0.0, 1.0); // a longer rest, a longer ring
            else if (in.duration < 0.25 && in.gapAfter > 0.02)
                dev -= 0.18 * (1.0 - in.duration / 0.25); // short, detached notes stop cleanly
            if (in.breathAfter)
                dev -= 0.08; // a breath cuts the note off
            write (c, i, c.centre + dev * scale);
        }
    }

    // Attack: softer only for quiet entries; the preset's attack otherwise.
    if (const auto& c = profile.attackControl; usable (c))
    {
        shaped.push_back ("Attack (CC" + std::to_string (c.controller) + ")");
        for (size_t i = 0; i < n; ++i)
        {
            const auto& in = info[i];
            double dev = 0.0;
            const bool entry = in.phraseStart || gapBefore (i) >= 0.25;
            if (entry && in.level < 0.5 && ! ctx.isShort (i))
                dev += 0.9 * (0.5 - in.level); // pp (0.24) gets about +0.23
            write (c, i, c.centre + dev * scale);
        }
    }

    // Legato blur: only on connected notes, where there's a join to blur.
    if (const auto& c = profile.legatoBlurControl; usable (c))
    {
        bool any = false;
        for (size_t i = 1; i < n; ++i)
        {
            if (! connected (i))
                continue;
            any = true;
            const auto& in = info[i];
            const double interval = in.onset - info[i - 1].onset;
            double dev = 0.0;
            if (interval > 0.6)
                dev += 0.12 * std::clamp ((interval - 0.6) / 1.0, 0.0, 1.0); // slow: let it blur
            else if (interval < 0.25)
                dev -= 0.12 * (1.0 - interval / 0.25); // fast: clean joins
            dev += 0.25 * (0.6 - in.level);             // soft: smoother
            write (c, i, c.centre + dev * scale);
        }
        if (any)
            shaped.push_back ("Legato blur (CC" + std::to_string (c.controller) + ")");
    }

    if (out.empty())
        return;
    part.controllers.insert (part.controllers.end(), out.begin(), out.end());
    std::stable_sort (part.controllers.begin(), part.controllers.end(),
                      [] (const ControllerEvent& a, const ControllerEvent& b) { return a.tick < b.tick; });
    std::string text;
    for (const auto& name : shaped)
        text += (text.empty() ? "" : ", ") + name;
    ctx.report.blends.push_back ("Shaped per note: " + text);
}

} // namespace vslf::detail
