#pragma once

#include <cstdint>

namespace vslp
{

enum class ChordResolution
{
    KeepHighest,
    KeepLowest
};

// User-facing controls. Amounts are 0..1 unless the name says otherwise;
// the "...Amount" controls go up to 2 for exaggerated results.
struct Settings
{
    // Master amount of shaping applied on top of the base dynamic.
    double intensity = 0.6;

    // Base dynamic. With useInputVelocity the incoming velocities are the
    // base level (so a notation export's pp/ff survive); otherwise baseLevel.
    bool useInputVelocity = true;
    double baseLevel = 0.6;    // 0..1, used when useInputVelocity is false
    double levelOffset = 0.0;  // -0.5..0.5, added to the base

    // Individual rules (0..2, 1 = default strength).
    double phraseArchAmount = 1.0;
    double highLoudAmount = 1.0;
    double durationContrastAmount = 1.0;
    double metricAccentAmount = 1.0;
    double longNoteSwellAmount = 1.0;
    double phraseEndTaperAmount = 1.0;

    // Humanization (deterministic: the same seed always gives the same result).
    double humanizeVelocity = 0.5; // 0..1
    double humanizeTimingMs = 6.0; // max +/- onset offset
    uint32_t seed = 1;

    // Output ranges.
    int velocityMin = 15;
    int velocityMax = 127;
    int dynamicsMin = 10;       // CC1 range
    int dynamicsMax = 127;
    int expressionBase = 127;   // CC11 resting value
    int expressionMin = 60;     // lowest CC11 used by tapers

    // Timing.
    double legatoOverlapMs = 40.0;   // overlap added between legato notes
    double legatoMaxGapMs = 80.0;    // gaps up to this are bridged in legato
    double repeatedNoteGapMs = 20.0; // gap before a repeated pitch (non-legato)
    double minNoteMs = 25.0;         // notes are never shortened below this

    // Controller output.
    double ccIntervalMs = 10.0; // curve resolution
    double ccLeadMs = 15.0;     // CC value lands this long before its note-on
    double breathMs = 150.0;    // gap left for a breath when a long wind/brass phrase is split

    ChordResolution chords = ChordResolution::KeepHighest;
};

} // namespace vslp
