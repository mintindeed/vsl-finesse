#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vslf/Engine.h>
#include <SynchronState.h>

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

    vslf::Part input;
    std::vector<std::string> readWarnings;
    vslf::InstrumentProfile profile;
    vslf::Settings settings;
    std::vector<vslf::Marking> markings; // pp..ff, crescendos (input ticks)
    std::vector<vslf::ArticulationMarking> articulationMarkings; // picked articulations (input ticks)
    vslf::Result result;

    juce::PluginDescription pluginDescription;
    bool hasPlugin = false;
    // Synchron Player preset last seen on this track. When it changes, the
    // instrument profile and preset folder follow it.
    juce::String synchronPreset;
    // The articulations the loaded Synchron preset switches by keyswitch, and
    // the one it was in when the preset was loaded (what the part switches
    // back to after a picked passage, where the file has no keyswitch).
    std::vector<vslf::io::SynchronArticulation> synchronArticulations;
    std::vector<int> articulationStartKeys;

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

    // The selected passage (selected track's input ticks), used by the
    // dynamics buttons. No selection = the whole part.
    bool hasSelection() const { return selectionEnd > selectionStart; }
    vslf::Tick selectionStart = 0, selectionEnd = 0;
    void setSelection (vslf::Tick start, vslf::Tick end);
    void clearSelection();

    // Longest part, in seconds.
    double lengthSeconds() const;

    // Removes all tracks and returns their plugins for destruction.
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> clear();

private:
    std::vector<std::unique_ptr<Track>> trackList;
    int selected = 0;
};
