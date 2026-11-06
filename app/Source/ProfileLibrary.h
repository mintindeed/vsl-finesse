#pragma once

#include <juce_core/juce_core.h>

#include <vslp/Profile.h>

#include <vector>

// Built-in instrument profiles, overridden by the user's own copies in
// ~/Library/Application Support/VSL Performer/Profiles (one JSON file per id).
// Edits made in the app (articulation modes, ranges) are saved there.
class ProfileLibrary
{
public:
    ProfileLibrary();

    void reload();

    const std::vector<vslp::InstrumentProfile>& all() const { return profiles; }
    const vslp::InstrumentProfile* find (const std::string& id) const;
    const vslp::InstrumentProfile& fallback() const;

    bool isCustomised (const std::string& id) const;
    bool save (const vslp::InstrumentProfile&, juce::String& error);
    bool revertToBuiltin (const std::string& id);

    juce::File folder() const { return userFolder; }

    static juce::File appDataFolder();

private:
    juce::File userFolder;
    std::vector<vslp::InstrumentProfile> profiles;
    std::vector<std::string> customised;
};
