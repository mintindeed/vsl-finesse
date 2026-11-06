#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vslp/Engine.h>

#include "PlaybackSequence.h"

#include <array>
#include <bitset>
#include <memory>
#include <vector>

// One part: a MIDI file, how to finesse it, and the instrument that plays it.
//
// Message-thread data (input, profile, settings, result) is only touched on
// the message thread. The `audio` block is shared with the audio thread and
// only changed while holding Session::audioLock.
class Track
{
public:
    explicit Track (juce::String trackName) : name (std::move (trackName)) {}

    juce::String name;
    juce::File midiFile;
    bool hasMidi = false;

    vslp::Part input;
    std::vector<std::string> readWarnings;
    vslp::InstrumentProfile profile;
    vslp::Settings settings;
    vslp::Result result;

    juce::PluginDescription pluginDescription;
    bool hasPlugin = false;

    struct Audio
    {
        std::unique_ptr<juce::AudioPluginInstance> plugin;
        std::shared_ptr<const PlaybackSequence> original, finessed;
        std::array<std::bitset<128>, 16> sounding; // notes currently on, per channel
        bool mute = false;
        bool solo = false;
        float gain = 1.0f;
    };
    Audio audio;

    // Median input velocity: the flat CC1 the original is auditioned with.
    int flatDynamics() const;
};

class Session : public juce::ChangeBroadcaster
{
public:
    Session();

    juce::CriticalSection audioLock;

    const std::vector<std::unique_ptr<Track>>& tracks() const { return trackList; }
    int numTracks() const { return (int) trackList.size(); }
    Track* track (int index) const;

    Track& addTrack();
    // Returns the removed track's plugin so it can be destroyed outside the lock.
    std::unique_ptr<juce::AudioPluginInstance> removeTrack (int index);

    int selectedIndex() const { return selected; }
    Track* selectedTrack() const { return track (selected); }
    void select (int index);
    int indexOf (const Track*) const;

    // Runs the engine for a track and hands the new sequences to the audio thread.
    void process (Track&);

    // Swaps in a new plugin; returns the old one (destroy it on the message thread).
    std::unique_ptr<juce::AudioPluginInstance> swapPlugin (Track&, std::unique_ptr<juce::AudioPluginInstance>);

    // Longest part, in seconds.
    double lengthSeconds() const;

    // Removes all tracks and returns their plugins for destruction.
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> clear();

private:
    std::vector<std::unique_ptr<Track>> trackList;
    int selected = 0;
};
