#include "vslp/Profile.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace vslp
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
    return 0;
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

const char* toString (DynamicsMode m)
{
    switch (m)
    {
        case DynamicsMode::Velocity:     return "velocity";
        case DynamicsMode::VelXFSustain: return "velxf-sus";
        case DynamicsMode::VelXF:        return "velxf";
    }
    return "velxf-sus";
}

static std::string lower (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return char (std::tolower (c)); });
    return s;
}

std::optional<DynamicsMode> dynamicsModeFromString (const std::string& text)
{
    const auto s = lower (text);
    if (s == "velocity")                                    return DynamicsMode::Velocity;
    if (s == "velxf-sus" || s == "velxf sus" || s == "velxfsustain") return DynamicsMode::VelXFSustain;
    if (s == "velxf")                                       return DynamicsMode::VelXF;
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

} // namespace vslp
