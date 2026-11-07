#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace vslf
{

// The kind of Synchron preset the part is played with. These are the three
// preset folders of the Studio Special Editions:
//
//   VelXFSustain - "VelXF sus - MOD": sustained patches crossfade dynamic
//                  layers with the mod wheel (CC1); short patches follow velocity.
//   VelXF        - "VelXF - MOD": every patch crossfades with CC1. Velocity is
//                  not a dynamic (it can pick an attack), so it is left alone.
//   Velocity     - "Velocity": velocity picks the dynamic layer; CC1 does nothing.
enum class PresetType
{
    VelXFSustain,
    VelXF,
    Velocity
};

const char* toString (PresetType);
const char* displayName (PresetType);
std::optional<PresetType> presetTypeFromString (const std::string&);

enum class ChordResolution
{
    KeepHighest,
    KeepLowest
};

// User-facing controls. Amounts are 0..1 unless the name says otherwise;
// the "...Amount" controls go up to 2 for exaggerated results.
struct Settings
{
    PresetType presetType = PresetType::VelXFSustain;

    // Notes shorter than this count as "short" (staccato and similar), which
    // follow velocity in VelXF sus presets.
    double shortNoteMs = 300.0;

    // Human pauses: breaths for winds and brass, a moment for the bow to
    // cross strings, and a tiny lift where a phrase ends without a rest.
    bool humanPauses = true;

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
    // Notes written connected (gap up to legatoMaxGapMs) overlap by
    // legatoOverlapMs, which triggers legato transitions in legato patches;
    // Synchron's mono patches simply hand over to the next note.
    double legatoOverlapMs = 40.0;
    double legatoMaxGapMs = 80.0;
    double repeatedNoteGapMs = 20.0; // gap before a repeated pitch (non-legato)
    double minNoteMs = 25.0;         // notes are never shortened below this

    // Controller output.
    double ccIntervalMs = 10.0; // curve resolution
    double ccLeadMs = 15.0;     // CC value lands this long before its note-on
    double breathMs = 150.0;    // gap left for a breath when a long wind/brass phrase is split

    ChordResolution chords = ChordResolution::KeepHighest;
};

} // namespace vslf
