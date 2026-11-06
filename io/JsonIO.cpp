#include "JsonIO.h"

#include <cmath>

namespace vslp::io
{

namespace
{

juce::var noteVar (int pitch) { return juce::String (noteName (pitch)); }

// Accepts 36 or "C1" (Logic naming, 60 = C3).
bool parseNote (const juce::var& v, int& out)
{
    if (v.isInt() || v.isInt64() || v.isDouble())
    {
        out = (int) v;
        return out >= 0 && out <= 127;
    }
    if (v.isString())
        if (auto n = parseNoteName (v.toString().toStdString()))
        {
            out = *n;
            return true;
        }
    return false;
}

juce::var articulationToJson (const Articulation& a)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", juce::String (a.name));
    if (std::abs (a.attackOffsetMs) > 1.0e-9)
        o->setProperty ("attackOffsetMs", a.attackOffsetMs);
    return juce::var (o);
}

bool articulationFromJson (const juce::var& v, Articulation& a, std::string& error)
{
    if (! v.isObject())
    {
        error = "articulation must be an object";
        return false;
    }
    if (v.hasProperty ("name"))
        a.name = v["name"].toString().toStdString();
    // "mode" and "legato" (from older versions) are ignored: the patch type
    // is now chosen per track, and connected notes always overlap.
    if (v.hasProperty ("attackOffsetMs"))
        a.attackOffsetMs = (double) v["attackOffsetMs"];
    return true;
}

bool writeJson (const juce::var& v, const juce::File& file, std::string& error)
{
    file.getParentDirectory().createDirectory();
    if (! file.replaceWithText (juce::JSON::toString (v, false) + "\n"))
    {
        error = "Couldn't write " + file.getFullPathName().toStdString();
        return false;
    }
    return true;
}

bool readJson (const juce::File& file, juce::var& v, std::string& error)
{
    if (! file.existsAsFile())
    {
        error = "File not found: " + file.getFullPathName().toStdString();
        return false;
    }
    const auto result = juce::JSON::parse (file.loadFileAsString(), v);
    if (result.failed())
    {
        error = file.getFileName().toStdString() + ": " + result.getErrorMessage().toStdString();
        return false;
    }
    return true;
}

} // namespace

juce::var profileToJson (const InstrumentProfile& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("id", juce::String (p.id));
    o->setProperty ("name", juce::String (p.name));
    o->setProperty ("library", juce::String (p.library));
    o->setProperty ("family", juce::String (toString (p.family)));

    juce::Array<juce::var> range { noteVar (p.lowestNote), noteVar (p.highestNote) };
    o->setProperty ("range", range);

    o->setProperty ("dynamicsCC", p.dynamicsCC);
    o->setProperty ("expressionCC", p.expressionCC);
    if (p.maxBreathSeconds > 0.0)
        o->setProperty ("maxBreathSeconds", p.maxBreathSeconds);

    juce::Array<juce::var> preserved;
    for (int cc : p.preservedControllers)
        preserved.add (cc);
    o->setProperty ("preservedControllers", preserved);

    o->setProperty ("default", articulationToJson (p.defaultArticulation));
    o->setProperty ("unmapped", articulationToJson (p.unmappedArticulation));

    juce::Array<juce::var> keys;
    for (const auto& k : p.keys)
    {
        auto* ko = new juce::DynamicObject();
        ko->setProperty ("note", noteVar (k.pitch));
        ko->setProperty ("level", k.level);
        keys.add (juce::var (ko));
    }
    o->setProperty ("keys", keys);

    juce::Array<juce::var> arts;
    for (const auto& m : p.articulations)
    {
        auto v = articulationToJson (m.articulation);
        juce::Array<juce::var> mk;
        for (int k : m.keys)
            mk.add (noteVar (k));
        v.getDynamicObject()->setProperty ("keys", mk);
        arts.add (v);
    }
    o->setProperty ("articulations", arts);

    return juce::var (o);
}

bool profileFromJson (const juce::var& v, InstrumentProfile& p, std::string& error)
{
    if (! v.isObject())
    {
        error = "profile must be a JSON object";
        return false;
    }

    if (v.hasProperty ("id"))       p.id = v["id"].toString().toStdString();
    if (v.hasProperty ("name"))     p.name = v["name"].toString().toStdString();
    if (v.hasProperty ("library"))  p.library = v["library"].toString().toStdString();
    if (v.hasProperty ("family"))
    {
        const auto f = familyFromString (v["family"].toString().toStdString());
        if (! f)
        {
            error = "unknown family '" + v["family"].toString().toStdString() + "'";
            return false;
        }
        p.family = *f;
    }

    if (v.hasProperty ("range"))
    {
        const auto* r = v["range"].getArray();
        if (r == nullptr || r->size() != 2 || ! parseNote ((*r)[0], p.lowestNote) || ! parseNote ((*r)[1], p.highestNote))
        {
            error = "range must be [lowest, highest], e.g. [\"G2\", \"C7\"]";
            return false;
        }
    }

    if (v.hasProperty ("dynamicsCC"))       p.dynamicsCC = (int) v["dynamicsCC"];
    if (v.hasProperty ("expressionCC"))     p.expressionCC = (int) v["expressionCC"];
    if (v.hasProperty ("maxBreathSeconds")) p.maxBreathSeconds = (double) v["maxBreathSeconds"];

    if (const auto* pc = v["preservedControllers"].getArray())
    {
        p.preservedControllers.clear();
        for (const auto& cc : *pc)
            p.preservedControllers.push_back ((int) cc);
    }

    if (v.hasProperty ("default") && ! articulationFromJson (v["default"], p.defaultArticulation, error))
        return false;
    if (v.hasProperty ("unmapped") && ! articulationFromJson (v["unmapped"], p.unmappedArticulation, error))
        return false;

    if (const auto* keys = v["keys"].getArray())
    {
        p.keys.clear();
        for (const auto& k : *keys)
        {
            KeyswitchKey key;
            const juce::var note = k.isObject() ? k["note"] : k;
            if (! parseNote (note, key.pitch))
            {
                error = "bad keyswitch note '" + note.toString().toStdString() + "'";
                return false;
            }
            if (k.isObject() && k.hasProperty ("level"))
                key.level = (int) k["level"];
            p.keys.push_back (key);
        }
    }

    if (const auto* arts = v["articulations"].getArray())
    {
        p.articulations.clear();
        for (const auto& a : *arts)
        {
            ArticulationMapping m;
            if (! articulationFromJson (a, m.articulation, error))
                return false;
            const auto* mk = a["keys"].getArray();
            if (mk == nullptr || mk->isEmpty())
            {
                error = "articulation '" + m.articulation.name + "' needs \"keys\"";
                return false;
            }
            for (const auto& k : *mk)
            {
                int pitch = 0;
                if (! parseNote (k, pitch))
                {
                    error = "bad key '" + k.toString().toStdString() + "' in articulation '" + m.articulation.name + "'";
                    return false;
                }
                m.keys.push_back (pitch);
                // Every key used by an articulation is a keyswitch.
                const bool known = std::any_of (p.keys.begin(), p.keys.end(), [pitch] (const KeyswitchKey& kk) { return kk.pitch == pitch; });
                if (! known)
                    p.keys.push_back ({ pitch, 0 });
            }
            p.articulations.push_back (m);
        }
    }

    return true;
}

bool loadProfile (const juce::File& file, InstrumentProfile& out, std::string& error)
{
    juce::var v;
    if (! readJson (file, v, error))
        return false;
    InstrumentProfile p;
    if (! profileFromJson (v, p, error))
    {
        error = file.getFileName().toStdString() + ": " + error;
        return false;
    }
    out = p;
    return true;
}

bool saveProfile (const InstrumentProfile& p, const juce::File& file, std::string& error)
{
    return writeJson (profileToJson (p), file, error);
}

#define VSLP_SETTINGS_FIELDS(X) \
    X (intensity) X (useInputVelocity) X (baseLevel) X (levelOffset) \
    X (phraseArchAmount) X (highLoudAmount) X (durationContrastAmount) X (metricAccentAmount) \
    X (longNoteSwellAmount) X (phraseEndTaperAmount) \
    X (humanizeVelocity) X (humanizeTimingMs) \
    X (velocityMin) X (velocityMax) X (dynamicsMin) X (dynamicsMax) X (expressionBase) X (expressionMin) \
    X (legatoOverlapMs) X (legatoMaxGapMs) X (repeatedNoteGapMs) X (minNoteMs) \
    X (ccIntervalMs) X (ccLeadMs) X (breathMs) X (shortNoteMs)

juce::var settingsToJson (const Settings& s)
{
    auto* o = new juce::DynamicObject();
#define X(field) o->setProperty (#field, s.field);
    VSLP_SETTINGS_FIELDS (X)
#undef X
    o->setProperty ("seed", (juce::int64) s.seed);
    o->setProperty ("presetType", juce::String (toString (s.presetType)));
    o->setProperty ("chords", s.chords == ChordResolution::KeepHighest ? "keep-highest" : "keep-lowest");
    return juce::var (o);
}

void settingsFromJson (const juce::var& v, Settings& s)
{
    if (! v.isObject())
        return;
#define X(field) \
    if (v.hasProperty (#field)) s.field = static_cast<decltype (s.field)> (v[#field]);
    VSLP_SETTINGS_FIELDS (X)
#undef X
    if (v.hasProperty ("seed"))
        s.seed = (uint32_t) (juce::int64) v["seed"];
    if (v.hasProperty ("presetType"))
        if (auto t = presetTypeFromString (v["presetType"].toString().toStdString()))
            s.presetType = *t;
    if (v.hasProperty ("chords"))
        s.chords = v["chords"].toString() == "keep-lowest" ? ChordResolution::KeepLowest : ChordResolution::KeepHighest;
}

bool loadSettings (const juce::File& file, Settings& s, std::string& error)
{
    juce::var v;
    if (! readJson (file, v, error))
        return false;
    settingsFromJson (v, s);
    return true;
}

bool saveSettings (const Settings& s, const juce::File& file, std::string& error)
{
    return writeJson (settingsToJson (s), file, error);
}

} // namespace vslp::io
