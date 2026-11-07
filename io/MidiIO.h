#pragma once

// Standard MIDI file <-> vslf::Part, using JUCE's MidiFile.

#include <juce_audio_basics/juce_audio_basics.h>

#include <vslf/Model.h>

#include <string>
#include <vector>

namespace vslf::io
{

struct ReadResult
{
    bool ok = false;
    std::string error;
    std::vector<std::string> warnings;
    Part part;
};

ReadResult readMidi (juce::InputStream& stream, const std::string& fallbackName);
ReadResult readMidiFile (const juce::File& file);

// Writes a type 1 file: track 0 holds tempo, time signatures and other meta
// events; track 1 holds the part (name, keyswitches, notes, controllers).
juce::MidiFile toMidiFile (const Part& part);
bool writeMidiFile (const Part& part, const juce::File& file, std::string& error);

// "Melody.mid" -> "Melody (Finessed).mid" in the same folder.
juce::File finessedFileFor (const juce::File& original);

} // namespace vslf::io
