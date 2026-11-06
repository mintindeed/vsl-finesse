#pragma once

#include <optional>
#include <string>
#include <vector>

namespace vslp
{

// How a VSL patch turns MIDI into dynamics. These correspond to the three
// kinds of patches in the Studio Special Editions:
//
//   Velocity      - note-on velocity picks the dynamic layer. CC1 does nothing.
//   VelXFSustain  - "VelXF sus - MOD": velocity picks the attack, the mod wheel
//                   (CC1) crossfades dynamic layers for the sustain portion.
//   VelXF         - "VelXF - MOD": the mod wheel crossfades dynamic layers for
//                   the whole note; velocity has little or no effect.
enum class DynamicsMode
{
    Velocity,
    VelXFSustain,
    VelXF
};

enum class Family
{
    Strings,
    Woodwinds,
    Brass,
    Percussion,
    Other
};

struct Articulation
{
    std::string name = "Default";
    DynamicsMode mode = DynamicsMode::VelXFSustain;

    // True for legato patches: consecutive notes are made to overlap so the
    // player triggers its legato transitions.
    bool legato = false;

    // Moves note-ons earlier (negative) or later (positive) to compensate for
    // slow-speaking samples. 0 = no change.
    double attackOffsetMs = 0.0;
};

// A keyswitch pitch and the level of the player's articulation tree it
// belongs to. Synchron presets can use one key per level (e.g. C1 picks
// "Long", C#2 picks the variation), so the current articulation is the
// combination of the latest key on each level. Single-level presets just
// leave everything at level 0.
struct KeyswitchKey
{
    int pitch = 0;
    int level = 0;
};

// Maps a combination of keys (one per level, any order) to an articulation.
struct ArticulationMapping
{
    std::vector<int> keys;
    Articulation articulation;
};

struct InstrumentProfile
{
    std::string id = "generic";
    std::string name = "Generic instrument";
    std::string library;
    Family family = Family::Other;

    // Playable range. Notes outside it are treated as keyswitches.
    int lowestNote = 0;
    int highestNote = 127;

    // Known keyswitch pitches (also needed for keyswitches inside the range).
    std::vector<KeyswitchKey> keys;
    std::vector<ArticulationMapping> articulations;

    // Articulation in effect before the first keyswitch.
    Articulation defaultArticulation;

    // Articulation for key combinations that aren't mapped.
    Articulation unmappedArticulation;

    int dynamicsCC = 1;
    int expressionCC = 11;

    // CCs that survive ingest. Everything else is wiped.
    std::vector<int> preservedControllers;

    // Wind and brass players have to breathe: phrases longer than this are
    // split. 0 = no limit.
    double maxBreathSeconds = 0.0;

    bool isKeyswitch (int pitch) const;
    int levelOf (int pitch) const;

    // Best mapping for the current key combination: an exact match, else the
    // most specific mapping whose keys are all held, else nullptr.
    const ArticulationMapping* findArticulation (const std::vector<int>& combination) const;
    const Articulation& resolve (const std::vector<int>& combination) const;

    bool preservesController (int cc) const;
};

const char* toString (DynamicsMode);
std::optional<DynamicsMode> dynamicsModeFromString (const std::string&);
const char* toString (Family);
std::optional<Family> familyFromString (const std::string&);

// Note names use Logic's convention (60 = C3): "C3", "C#2", "Db2", "C-1".
std::string noteName (int pitch);
std::optional<int> parseNoteName (const std::string&);

} // namespace vslp
