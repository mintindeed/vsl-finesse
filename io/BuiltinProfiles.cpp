#include "BuiltinProfiles.h"
#include "JsonIO.h"

#include <VslpProfiles.h>

#include <algorithm>

namespace vslp::io
{

std::vector<InstrumentProfile> builtinProfiles()
{
    std::vector<InstrumentProfile> result;

    for (int i = 0; i < VslpProfiles::namedResourceListSize; ++i)
    {
        int size = 0;
        const char* data = VslpProfiles::getNamedResource (VslpProfiles::namedResourceList[i], size);
        if (data == nullptr)
            continue;

        juce::var json;
        if (juce::JSON::parse (juce::String::fromUTF8 (data, size), json).failed())
            continue;

        InstrumentProfile p;
        std::string error;
        if (profileFromJson (json, p, error))
            result.push_back (p);
        else
            jassertfalse; // a bundled profile is broken
    }

    if (findProfile (result, "generic") == nullptr)
    {
        InstrumentProfile g;
        result.push_back (g);
    }

    std::stable_sort (result.begin(), result.end(), [] (const InstrumentProfile& a, const InstrumentProfile& b) {
        if (a.family != b.family)
            return (int) a.family < (int) b.family;
        return a.lowestNote > b.lowestNote; // high to low, score order within a family
    });
    return result;
}

const InstrumentProfile* findProfile (const std::vector<InstrumentProfile>& profiles, const std::string& id)
{
    for (const auto& p : profiles)
        if (p.id == id)
            return &p;
    return nullptr;
}

} // namespace vslp::io
