#pragma once

// JSON for instrument profiles and settings (via juce::JSON).

#include <juce_core/juce_core.h>

#include <vslp/Engine.h>
#include <vslp/Profile.h>
#include <vslp/Settings.h>

#include <string>

namespace vslp::io
{

juce::var profileToJson (const InstrumentProfile&);
bool profileFromJson (const juce::var&, InstrumentProfile& out, std::string& error);

bool loadProfile (const juce::File&, InstrumentProfile& out, std::string& error);
bool saveProfile (const InstrumentProfile&, const juce::File&, std::string& error);

juce::var settingsToJson (const Settings&);
// Missing keys keep their current values in `inOut`.
void settingsFromJson (const juce::var&, Settings& inOut);

juce::var markingsToJson (const std::vector<Marking>&);
std::vector<Marking> markingsFromJson (const juce::var&);

bool loadSettings (const juce::File&, Settings& inOut, std::string& error);
bool saveSettings (const Settings&, const juce::File&, std::string& error);

} // namespace vslp::io
