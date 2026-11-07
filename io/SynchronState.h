#pragma once

#include <juce_core/juce_core.h>

#include <vslf/Settings.h>

#include <optional>

// Reads and patches Synchron Player's saved state (the Audio Unit's state as
// the host stores it). The state is a binary property list whose
// "PluginState" entry is Zstandard-compressed JSON. See docs/DESIGN.md.
namespace vslf::io
{
    // The JSON inside a Synchron Player state, or nothing if the state isn't
    // in that format (e.g. another plug-in, or a VST3 state).
    std::optional<juce::String> synchronStateJson (const juce::MemoryBlock& state);

    // The preset loaded in Synchron Player, read from its preset path, e.g.
    // "Studio Special Editions/01 Solo strings/02 VelXF - MOD/01 Violin I - VelXF".
    struct SynchronPreset
    {
        juce::String path;       // empty: no preset loaded
        juce::String instrument; // "Violin I"
        std::optional<vslf::PresetType> type; // from the preset folder, if it's one of the three
    };
    SynchronPreset describeSynchronPreset (const juce::String& path);

    // The preset in a Synchron Player state, or nothing if the state isn't
    // Synchron Player's.
    std::optional<SynchronPreset> synchronPreset (const juce::MemoryBlock& state);

    // The articulations a Synchron Player preset can switch to by keyswitch,
    // read from its articulation tree (data.custom.sampler.rootNode).
    struct SynchronArticulation
    {
        juce::StringArray path; // {"Long notes", "Marcato"}
        std::vector<int> keys;  // one key per level, MIDI note numbers
        juce::String name() const { return path.joinIntoString (" > "); }
    };
    struct SynchronArticulations
    {
        std::vector<SynchronArticulation> choices; // in the tree's order
        std::vector<int> currentKeys;              // the selection saved in the state
    };

    // From the JSON inside the state (see synchronStateJson). Empty if it has
    // no articulation tree.
    SynchronArticulations synchronArticulationsFromJson (const juce::var& stateJson);
    // From a Synchron Player state, or nothing if the state isn't Synchron Player's.
    std::optional<SynchronArticulations> synchronArticulations (const juce::MemoryBlock& state);

    // If the state shows the Flow view with no Flow preset loaded, returns the
    // same state switched to the Precision view. Otherwise returns nothing:
    // the state is left as it is.
    std::optional<juce::MemoryBlock> withPrecisionView (const juce::MemoryBlock& state);
}
