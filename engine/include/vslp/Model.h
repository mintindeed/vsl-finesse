#pragma once

// Plain data model shared by the engine, the MIDI file layer and the app.
// No JUCE dependency: the engine can be built and tested anywhere.

#include <cstdint>
#include <string>
#include <vector>

namespace vslp
{

using Tick = int64_t;

struct Note
{
    Tick start = 0;
    Tick end = 0;
    int pitch = 60;    // MIDI note number (Logic naming: 60 = C3)
    int velocity = 100;
    int channel = 1;   // 1..16

    Tick length() const { return end - start; }
};

struct ControllerEvent
{
    Tick tick = 0;
    int controller = 0;
    int value = 0;
    int channel = 1;
};

// Any event the engine doesn't interpret (program change, pitch bend, meta text...).
// Stored as raw bytes so it can be written back unchanged.
struct RawEvent
{
    Tick tick = 0;
    std::vector<uint8_t> bytes;
};

struct TempoChange
{
    Tick tick = 0;
    double microsecondsPerQuarter = 500000.0; // 120 bpm
};

struct TimeSignature
{
    Tick tick = 0;
    int numerator = 4;
    int denominator = 4;
};

// Converts between ticks and seconds using the file's tempo map.
class TempoMap
{
public:
    TempoMap() : TempoMap (480, {}) {}
    TempoMap (int ppq, std::vector<TempoChange> changes);

    int ppq() const { return ppq_; }
    const std::vector<TempoChange>& changes() const { return changes_; }

    double secondsAt (double tick) const;
    double tickAt (double seconds) const;
    double bpmAt (double tick) const;

    // Number of ticks covering `ms` milliseconds starting at `tick`.
    double ticksForMs (double tick, double ms) const;

private:
    struct Segment
    {
        double tick;
        double seconds;
        double secondsPerTick;
    };

    int ppq_ = 480;
    std::vector<TempoChange> changes_;
    std::vector<Segment> segments_;
};

// One instrument part, as read from a MIDI file.
struct Part
{
    std::string name;
    int ppq = 480;
    TempoMap tempo;
    std::vector<TimeSignature> timeSignatures;

    std::vector<Note> notes;                  // performance notes
    std::vector<Note> keyswitches;            // articulation switches, preserved as-is
    std::vector<ControllerEvent> controllers; // CCs
    std::vector<RawEvent> channelEvents;      // other channel events, passed through
    std::vector<RawEvent> metaEvents;         // tempo / time sig / key sig / markers etc.
};

} // namespace vslp
