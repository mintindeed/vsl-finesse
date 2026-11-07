#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vslf/Model.h>
#include <vslf/Profile.h>

#include <memory>
#include <vector>

// A part flattened into time-ordered MIDI messages with times in seconds,
// ready for the audio thread. Immutable once built, shared via shared_ptr.
class PlaybackSequence
{
public:
    // `defaultDynamics` is sent for CC1 when chasing a position before any CC1
    // in the part (so a flat "original" doesn't inherit the finessed value).
    static std::shared_ptr<const PlaybackSequence> build (const vslf::Part& part,
                                                          const vslf::InstrumentProfile& profile,
                                                          int defaultDynamics);

    // Adds the events in [from, to) to `out`, at sample offsets within a block
    // of `numSamples` starting at `from`.
    void collect (double from, double to, double sampleRate, int numSamples, juce::MidiBuffer& out) const;

    // Adds messages that recreate the instrument state (keyswitches, controllers,
    // program, pitch bend) as it would be at time t.
    void chase (double t, juce::MidiBuffer& out) const;

    double lengthSeconds() const { return length; }
    const vslf::TempoMap& tempo() const { return tempoMap; }
    const std::vector<vslf::TimeSignature>& timeSignatures() const { return meters; }
    const std::vector<int>& channels() const { return usedChannels; }

private:
    struct Event
    {
        double seconds;
        juce::MidiMessage message;
        int keyswitchLevel; // -1 = not a keyswitch
    };

    std::vector<Event> events;
    vslf::TempoMap tempoMap;
    std::vector<vslf::TimeSignature> meters;
    std::vector<int> usedChannels;
    double length = 0.0;
    int dynamicsCC = 1;
    int expressionCC = 11;
    int defaultDynamicsValue = 100;
};
