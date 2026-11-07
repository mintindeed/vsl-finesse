#include "ProfileLibrary.h"

#include <BuiltinProfiles.h>
#include <JsonIO.h>

#include <algorithm>

juce::File ProfileLibrary::appDataFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    const auto folder = base.getChildFile ("VSL Finesse");

    // The app used to be called VSL Performer. Move its folder (saved
    // profiles, settings) across once, so nothing is lost after the rename.
    const auto legacy = base.getChildFile ("VSL Performer");
    if (! folder.exists() && legacy.isDirectory() && legacy.moveFileTo (folder))
    {
        const auto oldSettings = folder.getChildFile ("VSL Performer.settings");
        if (oldSettings.existsAsFile())
            oldSettings.moveFileTo (folder.getChildFile ("VSL Finesse.settings"));
    }
    return folder;
}

ProfileLibrary::ProfileLibrary()
    : userFolder (appDataFolder().getChildFile ("Profiles"))
{
    reload();
}

void ProfileLibrary::reload()
{
    profiles = vslf::io::builtinProfiles();
    customised.clear();

    for (const auto& file : userFolder.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        vslf::InstrumentProfile p;
        std::string error;
        if (! vslf::io::loadProfile (file, p, error))
        {
            DBG ("Skipping profile: " << error);
            continue;
        }

        auto it = std::find_if (profiles.begin(), profiles.end(), [&p] (const auto& q) { return q.id == p.id; });
        if (it != profiles.end())
            *it = p;
        else
            profiles.push_back (p);
        customised.push_back (p.id);
    }
}

const vslf::InstrumentProfile* ProfileLibrary::find (const std::string& id) const
{
    for (const auto& p : profiles)
        if (p.id == id)
            return &p;
    return nullptr;
}

const vslf::InstrumentProfile& ProfileLibrary::fallback() const
{
    if (const auto* g = find ("generic"))
        return *g;
    return profiles.front();
}

bool ProfileLibrary::isCustomised (const std::string& id) const
{
    return std::find (customised.begin(), customised.end(), id) != customised.end();
}

bool ProfileLibrary::save (const vslf::InstrumentProfile& p, juce::String& error)
{
    std::string e;
    if (! vslf::io::saveProfile (p, userFolder.getChildFile (juce::String (p.id) + ".json"), e))
    {
        error = e;
        return false;
    }

    auto it = std::find_if (profiles.begin(), profiles.end(), [&p] (const auto& q) { return q.id == p.id; });
    if (it != profiles.end())
        *it = p;
    else
        profiles.push_back (p);
    if (! isCustomised (p.id))
        customised.push_back (p.id);
    return true;
}

bool ProfileLibrary::revertToBuiltin (const std::string& id)
{
    const bool ok = userFolder.getChildFile (juce::String (id) + ".json").deleteFile();
    reload();
    return ok;
}
