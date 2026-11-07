#pragma once

// Internal pipeline types shared between engine stages.

#include "vslf/Engine.h"

#include <cstdint>

namespace vslf::detail
{

struct NoteInfo
{
    double onset = 0.0;    // seconds
    double offset = 0.0;   // seconds
    double duration = 0.0; // seconds
    double ioi = 0.0;      // onset-to-onset to next note (seconds), 0 for the last
    double gapAfter = 0.0; // silence after this note (seconds), large for the last
    double metricWeight = 0.0; // 0..1, 1 = downbeat
    double beatSeconds = 0.5;  // length of a beat here
    int articulation = 0;      // index into Context::articulations
    int phrase = 0;
    bool phraseStart = false;
    bool phraseEnd = false;
    bool breathAfter = false; // phrase split for breath (winds/brass), no written rest
    double pauseAfterMs = 0.0; // human pause taken from the end of this note
    int string = -1;           // string instruments: index into profile.openStrings
    double hairpin = 0.0;      // crescendo/diminuendo offset at the onset
    double phrasePosition = 0.0; // 0..1 within the phrase (by time)
    int inputVelocity = 100;     // as written in the file

    // Dynamics, 0..1 scale (1 = MIDI 127).
    double base = 0.6;  // base dynamic before shaping
    double level = 0.6; // shaped level at onset (drives CC1 and velocity)
    double velocity = 0.6;
    double swell = 0.0; // bump height for long notes
    double taper = 0.0; // drop over the end of phrase-final notes
};

struct Context
{
    const InstrumentProfile& profile;
    const Settings& settings;
    Part part; // ingested part (monophonic notes, keyswitches separated)
    std::vector<NoteInfo> info;
    std::vector<Articulation> articulations;
    std::vector<std::vector<int>> articulationKeys;
    std::vector<Phrase> phrases;
    Report report;
    std::vector<int> fileControllers; // controller numbers the file's kept CCs use

    // User markings, in seconds.
    struct TimedMarking
    {
        Marking::Kind kind;
        double start, end;
        int level;
    };
    std::vector<TimedMarking> markings;

    // Dynamic level from Dynamic markings at time t, if any covers it.
    bool markedLevel (double t, double& level) const;
    // Offset from crescendos / diminuendos at time t (0 if none).
    double hairpinAt (double t) const;

    const Articulation& articulationOf (size_t note) const { return articulations[(size_t) info[note].articulation]; }

    bool isShort (size_t note) const { return info[note].duration * 1000.0 < settings.shortNoteMs; }

    // How the player says this note's articulation picks its layers. The
    // Velocity preset type overrides it: that's the user's choice.
    Articulation::Layers layersOf (size_t note) const
    {
        return settings.presetType == PresetType::Velocity ? Articulation::Layers::Unknown : articulationOf (note).layers;
    }

    // Does CC1 drive this note's dynamic layers?
    bool crossfades (size_t note) const
    {
        if (const auto l = layersOf (note); l != Articulation::Layers::Unknown)
            return l == Articulation::Layers::Crossfade;
        switch (settings.presetType)
        {
            case PresetType::VelXF:        return true;
            case PresetType::VelXFSustain: return ! isShort (note);
            case PresetType::Velocity:     return false;
        }
        return true;
    }

    // Is velocity this note's dynamic? If not, the written velocity is kept
    // (it may select something else, such as an attack variant).
    bool velocityIsDynamic (size_t note) const
    {
        if (const auto l = layersOf (note); l != Articulation::Layers::Unknown)
            return l == Articulation::Layers::Velocity;
        switch (settings.presetType)
        {
            case PresetType::VelXF:        return false;
            case PresetType::VelXFSustain: return isShort (note);
            case PresetType::Velocity:     return true;
        }
        return false;
    }
};

// Deterministic noise in [-1, 1].
double noise (uint32_t seed, uint64_t a, uint64_t b);

double clamp01 (double v);
double smoothstep (double x);

void applyArticulationPicks (Context&, const ArticulationPicks&);
void assignArticulations (Context&);
void analyze (Context&);
void shapeDynamics (Context&);
void findPauses (Context&);
void applyTiming (Context&);
void writeControllers (Context&);
void writeBlends (Context&);

} // namespace vslf::detail
