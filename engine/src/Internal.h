#pragma once

// Internal pipeline types shared between engine stages.

#include "vslp/Engine.h"

#include <cstdint>

namespace vslp::detail
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
    double phrasePosition = 0.0; // 0..1 within the phrase (by time)

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

    const Articulation& articulationOf (size_t note) const { return articulations[(size_t) info[note].articulation]; }
};

// Deterministic noise in [-1, 1].
double noise (uint32_t seed, uint64_t a, uint64_t b);

double clamp01 (double v);
double smoothstep (double x);

void assignArticulations (Context&);
void analyze (Context&);
void shapeDynamics (Context&);
void applyTiming (Context&);
void writeControllers (Context&);

} // namespace vslp::detail
