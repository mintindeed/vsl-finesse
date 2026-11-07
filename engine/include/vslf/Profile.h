#pragma once

#include <optional>
#include <string>
#include <vector>

namespace vslf
{

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

    // Moves note-ons earlier (negative) or later (positive) to compensate for
    // slow-speaking samples. 0 = no change.
    double attackOffsetMs = 0.0;

    // How the player picks this articulation's dynamic layers, when it says
    // so (Synchron presets do, per patch). Unknown: the preset type and the
    // note's length decide.
    enum class Layers
    {
        Unknown,
        Velocity,  // velocity picks the layer
        Crossfade  // CC1 crossfades the layers; velocity is kept as written
    };
    Layers layers = Layers::Unknown;

    // A crossfaded dimension under this articulation: a Dimension Control
    // (Dim.Ctrl/A-H in Synchron Player) mixes the slots of a parallel
    // dimension, and the engine shapes it like a player would.
    struct Blend
    {
        enum class Kind
        {
            None,
            // One slot always plays and the control fades another in on top
            // (Marcato: sustain + staccato attack). An accent: one value per
            // note, set before it starts.
            AddedLayer,
            // The control turns one sound into another (XF tremolo). Colour:
            // a smooth line that follows the phrase.
            Crossfade
        };
        Kind kind = Kind::None;
        int controller = -1;  // the CC it's written to; -1 = the note's velocity
        double centre = 0.5;  // the preset's own value for the control, 0..1
        std::string name;     // the dimension's name, e.g. "Attack", "XF tremolo"
    };
    Blend blend;
};

// A keyswitch pitch and the level of the player's articulation tree it
// belongs to. Synchron presets use one octave per level (e.g. C0-E0 pick the
// articulation, C1-C#1 the type, in Logic's naming), so the current articulation is the
// combination of the latest key on each level. Keys not listed here get
// their octave as their level.
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

    // Open strings, low to high (string instruments only). Used to work out
    // which string each note is played on, so string crossings get time.
    std::vector<int> openStrings;

    // The player's performance controls, where the loaded preset has them
    // (Synchron Player: Attack CC22, Release CC23, Legato blur CC33).
    // controller -1: not available, nothing is written. centre: the preset's
    // own value, 0..1.
    struct PerformControl
    {
        int controller = -1;
        double centre = 0.0;
    };
    PerformControl attackControl, releaseControl, legatoBlurControl;

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

const char* toString (Family);
std::optional<Family> familyFromString (const std::string&);

// The profile an instrument name refers to ("Violin I", "Double Bass",
// "Bass Clarinet.mid"), or nullptr. Looks for each profile's name, its
// singular and its id in the text, and prefers the longest match, so "bass
// clarinet" beats "clarinet". The generic profile is never picked.
const InstrumentProfile* guessProfile (const std::string& text, const std::vector<InstrumentProfile>& profiles);

// Note names use Logic's convention (60 = C3): "C3", "C#2", "Db2", "C-1".
std::string noteName (int pitch);
std::optional<int> parseNoteName (const std::string&);

} // namespace vslf
