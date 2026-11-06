// vslp-finesse: the engine on the command line.
//
//   vslp-finesse Melody.mid --instrument violins
//   vslp-finesse Melody.mid -p my-violins.json -s settings.json -o out.mid
//   vslp-finesse --list-instruments

#include <juce_core/juce_core.h>

#include <BuiltinProfiles.h>
#include <JsonIO.h>
#include <MidiIO.h>
#include <vslp/Engine.h>

#include <iostream>
#include <optional>

using namespace vslp;

static void usage()
{
    std::cout <<
        "usage: vslp-finesse <input.mid> [options]\n"
        "\n"
        "  -o, --output <file.mid>     output file (default: \"<input> (Finessed).mid\")\n"
        "  -i, --instrument <id>       built-in instrument profile (default: generic)\n"
        "  -p, --profile <file.json>   instrument profile file (overrides --instrument)\n"
        "  -s, --settings <file.json>  settings file\n"
        "      --intensity <0..2>      overall amount of shaping\n"
        "      --seed <n>              humanization seed\n"
        "      --list-instruments      show built-in profiles\n"
        "      --dump-profile <id>     print a built-in profile as JSON (a starting point for your own)\n"
        "      --dump-settings         print the default settings as JSON\n"
        "  -q, --quiet                 no report\n";
}

int main (int argc, char* argv[])
{

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    const auto profiles = io::builtinProfiles();

    juce::String input, output, instrumentId = "generic", profileFile, settingsFile;
    Settings settings;
    bool quiet = false;
    std::optional<double> intensity;
    std::optional<uint32_t> seed;

    for (int i = 0; i < args.size(); ++i)
    {
        const auto& a = args[i];
        auto next = [&] () -> juce::String {
            if (i + 1 >= args.size())
            {
                std::cerr << "missing value for " << a << "\n";
                std::exit (2);
            }
            return args[++i];
        };

        if (a == "-h" || a == "--help")               { usage(); return 0; }
        else if (a == "-o" || a == "--output")        output = next();
        else if (a == "-i" || a == "--instrument")    instrumentId = next();
        else if (a == "-p" || a == "--profile")       profileFile = next();
        else if (a == "-s" || a == "--settings")      settingsFile = next();
        else if (a == "--intensity")                  intensity = next().getDoubleValue();
        else if (a == "--seed")                       seed = (uint32_t) next().getLargeIntValue();
        else if (a == "-q" || a == "--quiet")         quiet = true;
        else if (a == "--list-instruments")
        {
            for (const auto& p : profiles)
                std::cout << p.id << "\t" << p.name << "\t" << noteName (p.lowestNote) << "-" << noteName (p.highestNote) << "\n";
            return 0;
        }
        else if (a == "--dump-profile")
        {
            const auto id = next().toStdString();
            if (const auto* p = io::findProfile (profiles, id))
            {
                std::cout << juce::JSON::toString (io::profileToJson (*p)) << "\n";
                return 0;
            }
            std::cerr << "unknown instrument '" << id << "'\n";
            return 2;
        }
        else if (a == "--dump-settings")
        {
            std::cout << juce::JSON::toString (io::settingsToJson (Settings {})) << "\n";
            return 0;
        }
        else if (a.startsWith ("-"))
        {
            std::cerr << "unknown option " << a << "\n";
            usage();
            return 2;
        }
        else
            input = a;
    }

    if (input.isEmpty())
    {
        usage();
        return 2;
    }

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto inputFile = cwd.getChildFile (input);

    InstrumentProfile profile;
    std::string error;
    if (profileFile.isNotEmpty())
    {
        if (! io::loadProfile (cwd.getChildFile (profileFile), profile, error))
        {
            std::cerr << error << "\n";
            return 1;
        }
    }
    else if (const auto* p = io::findProfile (profiles, instrumentId.toStdString()))
        profile = *p;
    else
    {
        std::cerr << "unknown instrument '" << instrumentId << "' (see --list-instruments)\n";
        return 2;
    }

    if (settingsFile.isNotEmpty() && ! io::loadSettings (cwd.getChildFile (settingsFile), settings, error))
    {
        std::cerr << error << "\n";
        return 1;
    }
    if (intensity) settings.intensity = *intensity;
    if (seed)      settings.seed = *seed;

    auto read = io::readMidiFile (inputFile);
    if (! read.ok)
    {
        std::cerr << read.error << "\n";
        return 1;
    }

    const auto result = finesse (read.part, profile, settings);

    const auto outFile = output.isNotEmpty() ? cwd.getChildFile (output) : io::finessedFileFor (inputFile);
    if (! io::writeMidiFile (result.part, outFile, error))
    {
        std::cerr << error << "\n";
        return 1;
    }

    if (! quiet)
    {
        const auto& r = result.report;
        std::cout << "Part:        " << result.part.name << " (" << profile.name << ")\n"
                  << "Notes:       " << r.notesIn << " in, " << r.notesOut << " out ("
                  << r.chordNotesRemoved << " chord notes removed, " << r.overlapsTrimmed << " overlaps trimmed)\n"
                  << "Keyswitches: " << r.keyswitchesIn << " kept\n"
                  << "Phrases:     " << result.phrases.size() << "\n"
                  << "Legato:      " << r.legatoConnections << " connections\n"
                  << "CCs:         " << r.controllersRemoved << " removed, "
                  << result.part.controllers.size() << " written\n";
        for (const auto& u : r.articulations)
        {
            std::string keys;
            for (int k : u.keys)
                keys += (keys.empty() ? "" : "+") + noteName (k);
            std::cout << "  " << (keys.empty() ? "(start)" : keys) << ": " << u.articulation << ", " << u.notes << " notes\n";
        }
        for (const auto& w : read.warnings)
            std::cout << "warning: " << w << "\n";
        for (const auto& w : r.warnings)
            std::cout << "warning: " << w << "\n";
        std::cout << "Wrote " << outFile.getFullPathName() << "\n";
    }
    return 0;
}
