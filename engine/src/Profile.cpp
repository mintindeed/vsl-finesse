#include "vslf/Profile.h"
#include "vslf/Settings.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace vslf
{

bool InstrumentProfile::isKeyswitch (int pitch) const
{
    for (const auto& k : keys)
        if (k.pitch == pitch)
            return true;
    return pitch < lowestNote || pitch > highestNote;
}

int InstrumentProfile::levelOf (int pitch) const
{
    for (const auto& k : keys)
        if (k.pitch == pitch)
            return k.level;
    return pitch / 12; // one octave per level, as in Synchron presets
}

const ArticulationMapping* InstrumentProfile::findArticulation (const std::vector<int>& combination) const
{
    auto sorted = [] (std::vector<int> v) { std::sort (v.begin(), v.end()); return v; };
    const auto combo = sorted (combination);

    const ArticulationMapping* best = nullptr;
    for (const auto& m : articulations)
    {
        const auto mk = sorted (m.keys);
        if (mk == combo)
            return &m;
        if (! mk.empty() && std::includes (combo.begin(), combo.end(), mk.begin(), mk.end()))
            if (best == nullptr || mk.size() > best->keys.size())
                best = &m;
    }
    return best;
}

const Articulation& InstrumentProfile::resolve (const std::vector<int>& combination) const
{
    if (combination.empty())
        return defaultArticulation;
    if (const auto* m = findArticulation (combination))
        return m->articulation;
    return unmappedArticulation;
}

bool InstrumentProfile::preservesController (int cc) const
{
    return std::find (preservedControllers.begin(), preservedControllers.end(), cc) != preservedControllers.end();
}

const char* toString (PresetType t)
{
    switch (t)
    {
        case PresetType::VelXFSustain: return "velxf-sus";
        case PresetType::VelXF:        return "velxf";
        case PresetType::Velocity:     return "velocity";
    }
    return "velxf-sus";
}

const char* displayName (PresetType t)
{
    switch (t)
    {
        case PresetType::VelXFSustain: return "VelXF sus - MOD";
        case PresetType::VelXF:        return "VelXF - MOD";
        case PresetType::Velocity:     return "Velocity";
    }
    return "VelXF sus - MOD";
}

std::optional<PresetType> presetTypeFromString (const std::string& text)
{
    std::string s;
    for (char c : text)
        if (std::isalnum ((unsigned char) c))
            s += (char) std::tolower ((unsigned char) c);
    if (s == "velxfsus" || s == "velxfsusmod" || s == "velxfsustain") return PresetType::VelXFSustain;
    if (s == "velxf" || s == "velxfmod")                             return PresetType::VelXF;
    if (s == "velocity")                                             return PresetType::Velocity;
    return std::nullopt;
}

const char* toString (Family f)
{
    switch (f)
    {
        case Family::Strings:    return "strings";
        case Family::Woodwinds:  return "woodwinds";
        case Family::Brass:      return "brass";
        case Family::Percussion: return "percussion";
        case Family::Other:      return "other";
    }
    return "other";
}

static std::string lower (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return char (std::tolower (c)); });
    return s;
}

std::optional<Family> familyFromString (const std::string& text)
{
    const auto s = lower (text);
    if (s == "strings")    return Family::Strings;
    if (s == "woodwinds")  return Family::Woodwinds;
    if (s == "brass")      return Family::Brass;
    if (s == "percussion") return Family::Percussion;
    if (s == "other")      return Family::Other;
    return std::nullopt;
}

std::string noteName (int pitch)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int octave = pitch / 12 - 2; // Logic: 60 = C3
    return std::string (names[((pitch % 12) + 12) % 12]) + std::to_string (octave);
}

std::optional<int> parseNoteName (const std::string& text)
{
    size_t i = 0;
    while (i < text.size() && std::isspace ((unsigned char) text[i]))
        ++i;
    if (i >= text.size())
        return std::nullopt;

    static const int semitone[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    const char letter = (char) std::toupper ((unsigned char) text[i]);
    if (letter < 'A' || letter > 'G')
        return std::nullopt;
    int pitch = semitone[letter - 'A'];
    ++i;

    if (i < text.size() && (text[i] == '#' || text[i] == 'b'))
    {
        pitch += text[i] == '#' ? 1 : -1;
        ++i;
    }

    if (i >= text.size())
        return std::nullopt;

    try
    {
        size_t used = 0;
        const int octave = std::stoi (text.substr (i), &used);
        if (i + used != text.size())
            return std::nullopt;
        const int midi = (octave + 2) * 12 + pitch;
        if (midi < 0 || midi > 127)
            return std::nullopt;
        return midi;
    }
    catch (...)
    {
        return std::nullopt;
    }
}


const InstrumentProfile* guessProfile (const std::string& text, const std::vector<InstrumentProfile>& profiles)
{
    auto lower = [] (std::string t) {
        for (auto& c : t)
            c = (char) std::tolower ((unsigned char) c);
        return t;
    };
    auto endsWith = [] (const std::string& t, const std::string& end) {
        return t.size() > end.size() && t.compare (t.size() - end.size(), end.size(), end) == 0;
    };

    const auto haystack = lower (text);
    const InstrumentProfile* best = nullptr;
    size_t bestLength = 0;
    for (const auto& p : profiles)
    {
        if (p.id == "generic")
            continue;
        const auto name = lower (p.name);
        std::vector<std::string> terms { name, lower (p.id) };
        if (endsWith (name, "es"))
            terms.push_back (name.substr (0, name.size() - 2)); // basses -> bass
        if (endsWith (name, "s"))
            terms.push_back (name.substr (0, name.size() - 1)); // violins -> violin
        for (const auto& term : terms)
            if (term.size() > bestLength && haystack.find (term) != std::string::npos)
            {
                best = &p;
                bestLength = term.size();
            }
    }
    return best;
}

} // namespace vslf
