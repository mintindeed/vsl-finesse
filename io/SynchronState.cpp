#include "SynchronState.h"

#include <zstd.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

namespace vslf::io
{
namespace
{
    using Bytes = std::vector<uint8_t>;

    // --- Binary property list ---------------------------------------------
    // Just enough of Apple's bplist00 format to find one data entry in the
    // top-level dictionary and replace it. Objects refer to each other by
    // index, not by position, so every other object is copied byte for byte
    // and only the offset table is rebuilt.
    struct BinaryPlist
    {
        const Bytes& bytes;
        int offsetSize = 0, refSize = 0;
        uint64_t numObjects = 0, topObject = 0, tableOffset = 0;
        std::vector<uint64_t> offsets;

        explicit BinaryPlist (const Bytes& b) : bytes (b) {}

        bool readUInt (uint64_t pos, int size, uint64_t& out) const
        {
            if (size < 1 || size > 8 || pos + (uint64_t) size > bytes.size())
                return false;
            out = 0;
            for (int i = 0; i < size; ++i)
                out = (out << 8) | bytes[(size_t) pos + (size_t) i];
            return true;
        }

        bool parse()
        {
            if (bytes.size() < 8 + 32 || std::memcmp (bytes.data(), "bplist00", 8) != 0)
                return false;
            const auto t = (uint64_t) bytes.size() - 32;
            offsetSize = bytes[(size_t) t + 6];
            refSize = bytes[(size_t) t + 7];
            if (! readUInt (t + 8, 8, numObjects) || ! readUInt (t + 16, 8, topObject) || ! readUInt (t + 24, 8, tableOffset))
                return false;
            if (numObjects == 0 || topObject >= numObjects || refSize < 1 || refSize > 8
                || tableOffset + numObjects * (uint64_t) offsetSize > t)
                return false;
            offsets.resize ((size_t) numObjects);
            for (uint64_t i = 0; i < numObjects; ++i)
                if (! readUInt (tableOffset + i * (uint64_t) offsetSize, offsetSize, offsets[(size_t) i]) || offsets[(size_t) i] < 8
                    || offsets[(size_t) i] >= tableOffset)
                    return false;
            return true;
        }

        // An object's marker byte, and its element count (which may follow the
        // marker as an integer object). pos is moved past them.
        bool readHeader (uint64_t& pos, int& type, uint64_t& count) const
        {
            if (pos >= bytes.size())
                return false;
            const auto marker = bytes[(size_t) pos++];
            type = marker >> 4;
            count = marker & 0x0f;
            if (count != 0x0f)
                return true;
            if (pos >= bytes.size() || (bytes[(size_t) pos] >> 4) != 0x1)
                return false;
            const int size = 1 << (bytes[(size_t) pos] & 0x0f);
            ++pos;
            if (! readUInt (pos, size, count))
                return false;
            pos += (uint64_t) size;
            return true;
        }

        bool isAsciiString (uint64_t index, const char* text) const
        {
            auto pos = offsets[(size_t) index];
            int type = 0;
            uint64_t count = 0;
            const auto length = std::strlen (text);
            return readHeader (pos, type, count) && type == 0x5 && count == length && pos + count <= bytes.size()
                   && std::memcmp (bytes.data() + pos, text, length) == 0;
        }

        // Index of the value stored under key in the top-level dictionary.
        std::optional<uint64_t> topValue (const char* key) const
        {
            auto pos = offsets[(size_t) topObject];
            int type = 0;
            uint64_t count = 0;
            if (! readHeader (pos, type, count) || type != 0xd)
                return std::nullopt;
            for (uint64_t i = 0; i < count; ++i)
            {
                uint64_t keyRef = 0, valueRef = 0;
                if (! readUInt (pos + i * (uint64_t) refSize, refSize, keyRef)
                    || ! readUInt (pos + (count + i) * (uint64_t) refSize, refSize, valueRef)
                    || keyRef >= numObjects || valueRef >= numObjects)
                    return std::nullopt;
                if (isAsciiString (keyRef, key))
                    return valueRef;
            }
            return std::nullopt;
        }

        std::optional<Bytes> dataObject (uint64_t index) const
        {
            auto pos = offsets[(size_t) index];
            int type = 0;
            uint64_t count = 0;
            if (! readHeader (pos, type, count) || type != 0x4 || pos + count > bytes.size())
                return std::nullopt;
            return Bytes (bytes.begin() + (std::ptrdiff_t) pos, bytes.begin() + (std::ptrdiff_t) (pos + count));
        }

        // The file again, with object `index` replaced by `replacement` (an
        // encoded object).
        Bytes withObject (uint64_t index, const Bytes& replacement) const
        {
            // Each object runs up to the next object (or the offset table).
            auto sorted = offsets;
            std::sort (sorted.begin(), sorted.end());
            auto endOf = [&] (uint64_t start) {
                const auto next = std::upper_bound (sorted.begin(), sorted.end(), start);
                return next == sorted.end() ? tableOffset : *next;
            };

            Bytes out (bytes.begin(), bytes.begin() + 8);
            std::vector<uint64_t> newOffsets ((size_t) numObjects);
            for (uint64_t i = 0; i < numObjects; ++i)
            {
                newOffsets[(size_t) i] = out.size();
                if (i == index)
                    out.insert (out.end(), replacement.begin(), replacement.end());
                else
                    out.insert (out.end(), bytes.begin() + (std::ptrdiff_t) offsets[(size_t) i],
                                bytes.begin() + (std::ptrdiff_t) endOf (offsets[(size_t) i]));
            }

            const auto newTableOffset = (uint64_t) out.size();
            int newOffsetSize = 1;
            while (newOffsetSize < 8 && (newTableOffset >> (8 * newOffsetSize)) != 0)
                newOffsetSize *= 2;

            auto putUInt = [&out] (uint64_t v, int size) {
                for (int i = size - 1; i >= 0; --i)
                    out.push_back ((uint8_t) (v >> (8 * i)));
            };
            for (auto o : newOffsets)
                putUInt (o, newOffsetSize);

            out.insert (out.end(), 6, 0);
            out.push_back ((uint8_t) newOffsetSize);
            out.push_back ((uint8_t) refSize);
            putUInt (numObjects, 8);
            putUInt (topObject, 8);
            putUInt (newTableOffset, 8);
            return out;
        }
    };

    Bytes encodeData (const Bytes& data)
    {
        Bytes out;
        const auto n = (uint64_t) data.size();
        if (n < 15)
            out.push_back ((uint8_t) (0x40 | n));
        else
        {
            out.push_back (0x4f);
            const int power = n <= 0xff ? 0 : n <= 0xffff ? 1 : n <= 0xffffffffull ? 2 : 3;
            out.push_back ((uint8_t) (0x10 | power));
            for (int i = (1 << power) - 1; i >= 0; --i)
                out.push_back ((uint8_t) (n >> (8 * i)));
        }
        out.insert (out.end(), data.begin(), data.end());
        return out;
    }

    // --- Zstandard --------------------------------------------------------
    std::optional<Bytes> decompress (const Bytes& in)
    {
        auto* stream = ZSTD_createDStream();
        if (stream == nullptr)
            return std::nullopt;

        Bytes out;
        std::vector<uint8_t> chunk (ZSTD_DStreamOutSize());
        ZSTD_inBuffer input { in.data(), in.size(), 0 };
        size_t result = 1;
        while (input.pos < input.size)
        {
            ZSTD_outBuffer output { chunk.data(), chunk.size(), 0 };
            result = ZSTD_decompressStream (stream, &output, &input);
            if (ZSTD_isError (result) || out.size() + output.pos > (64u << 20))
                break;
            out.insert (out.end(), chunk.begin(), chunk.begin() + (std::ptrdiff_t) output.pos);
        }
        ZSTD_freeDStream (stream);
        if (ZSTD_isError (result) || result != 0) // 0: a whole frame was read
            return std::nullopt;
        return out;
    }

    std::optional<Bytes> compress (const juce::String& text)
    {
        const auto* src = text.toRawUTF8();
        const auto size = text.getNumBytesAsUTF8();
        Bytes out (ZSTD_compressBound (size));
        const auto written = ZSTD_compress (out.data(), out.size(), src, size, 3);
        if (ZSTD_isError (written))
            return std::nullopt;
        out.resize (written);
        return out;
    }

    Bytes toBytes (const juce::MemoryBlock& m)
    {
        const auto* p = static_cast<const uint8_t*> (m.getData());
        return Bytes (p, p + m.getSize());
    }

    // The preset's velXFadeEnabled: whether it crossfades dynamic layers
    // (with CC1 in the MOD presets) or lets velocity pick them. Nothing if
    // the state doesn't say.
    std::optional<bool> crossfadingOf (const juce::var& stateJson)
    {
        const auto v = stateJson["data"]["parameters"]["velXFadeEnabled"];
        if (v.isVoid() || v.isUndefined())
            return std::nullopt;
        return (double) v >= 0.5;
    }

    std::optional<Bytes> pluginStateJsonBytes (const BinaryPlist& plist)
    {
        if (const auto index = plist.topValue ("PluginState"))
            if (const auto data = plist.dataObject (*index))
                return decompress (*data);
        return std::nullopt;
    }
}

std::optional<juce::String> synchronStateJson (const juce::MemoryBlock& state)
{
    const auto bytes = toBytes (state);
    BinaryPlist plist (bytes);
    if (! plist.parse())
        return std::nullopt;
    const auto json = pluginStateJsonBytes (plist);
    if (! json)
        return std::nullopt;
    return juce::String::fromUTF8 (reinterpret_cast<const char*> (json->data()), (int) json->size());
}

std::optional<juce::MemoryBlock> withPrecisionView (const juce::MemoryBlock& state)
{
    const auto bytes = toBytes (state);
    BinaryPlist plist (bytes);
    if (! plist.parse())
        return std::nullopt;
    const auto index = plist.topValue ("PluginState");
    if (! index)
        return std::nullopt;
    const auto raw = plist.dataObject (*index);
    if (! raw)
        return std::nullopt;
    const auto jsonBytes = decompress (*raw);
    if (! jsonBytes)
        return std::nullopt;
    const auto json = juce::String::fromUTF8 (reinterpret_cast<const char*> (jsonBytes->data()), (int) jsonBytes->size());

    // Only an empty Flow view is switched: a Flow preset that's loaded stays.
    const auto parsed = juce::JSON::parse (json);
    const auto data = parsed["data"];
    const auto gui = data["meta"]["gui"];
    if (gui["currentSkin"].toString() != "Flow" || data["currentPreset"].toString().startsWith ("$FLOW"))
        return std::nullopt;

    // Change just the one value in the text, so everything else Synchron
    // wrote (number formats, key order) stays exactly as it was.
    const juce::String key ("\"currentSkin\"");
    const int at = json.indexOf (key);
    if (at < 0 || json.indexOf (at + 1, key) >= 0)
        return std::nullopt;
    auto p = json.getCharPointer() + (at + key.length());
    int pos = at + key.length();
    auto skipSpace = [&] { while (p.isWhitespace()) { ++p; ++pos; } };
    skipSpace();
    if (*p != ':')
        return std::nullopt;
    ++p;
    ++pos;
    skipSpace();
    const juce::String flow ("\"Flow\"");
    if (json.substring (pos, pos + flow.length()) != flow)
        return std::nullopt;
    const auto patched = json.replaceSection (pos, flow.length(), "\"SynchronPlayer\"");

    const auto packed = compress (patched);
    if (! packed)
        return std::nullopt;
    const auto out = plist.withObject (*index, encodeData (*packed));
    return juce::MemoryBlock (out.data(), out.size());
}

SynchronPreset describeSynchronPreset (const juce::String& path)
{
    SynchronPreset preset;
    preset.path = path;
    if (path.isEmpty())
        return preset;

    // "02 VelXF - MOD" -> "VelXF - MOD": the folders and presets are numbered.
    auto withoutNumber = [] (const juce::String& part) {
        return part.trim().trimCharactersAtStart ("0123456789").trim();
    };

    auto parts = juce::StringArray::fromTokens (path, "/", "");
    parts.removeEmptyStrings();
    if (parts.isEmpty())
        return preset;

    // "01 Violin I - VelXF" -> "Violin I"
    auto name = withoutNumber (parts[parts.size() - 1]);
    if (name.contains (" - "))
        name = name.upToLastOccurrenceOf (" - ", false, false).trim();
    preset.instrument = name;

    for (int i = parts.size() - 2; i >= 0 && ! preset.type; --i)
        preset.type = vslf::presetTypeFromString (withoutNumber (parts[i]).toStdString());
    return preset;
}

std::optional<SynchronPreset> synchronPreset (const juce::MemoryBlock& state)
{
    const auto json = synchronStateJson (state);
    if (! json)
        return std::nullopt;
    const auto parsed = juce::JSON::parse (*json);
    if (! parsed["data"].isObject())
        return std::nullopt;
    auto preset = describeSynchronPreset (parsed["data"]["currentPreset"].toString());
    if (! preset.type && preset.path.isNotEmpty())
        if (const auto crossfading = crossfadingOf (parsed))
            preset.type = *crossfading ? vslf::PresetType::VelXFSustain : vslf::PresetType::Velocity;
    return preset;
}

// --- Articulation tree --------------------------------------------------------
// Each node of the tree has a title and either children (`nodes`) or a patch.
// A node with children is a level of the tree: its `switchControl` says what
// picks the child. 0 is nothing, 1-128 a keyswitch (note + 1, the first child's
// key; with `whiteNoteSwitch` the children use white keys only), then the 128
// controllers, then from 257 the node crossfade slots (257 is slot 0,
// nodeXFade0, 258 slot 1). What drives a slot is set per preset in
// paramControl.mappings: Marcato's Attack slot is velocity in the VelXF
// presets but CC1 in the Velocity ones.
// `currentNodeIndex` is the child that's selected. Only key-switched levels
// are walked: a node below which no key picks anything is one articulation.
namespace
{
    constexpr int switchNoteFirst = 1, switchNoteLast = 128;

    bool hasPatch (const juce::var& node)
    {
        if (node["nodes"].size() > 0)
        {
            for (const auto& child : *node["nodes"].getArray())
                if (hasPatch (child))
                    return true;
            return false;
        }
        return node["patchName"].toString().isNotEmpty() || node["patchEntry"].isString();
    }

    std::optional<int> firstKey (const juce::var& node)
    {
        if (! node["nodes"].isArray() || node["nodes"].size() == 0)
            return std::nullopt;
        const int control = (int) node["switchControl"];
        if (control < switchNoteFirst || control > switchNoteLast)
            return std::nullopt;
        return control - switchNoteFirst;
    }

    // The key for child `index` of a level whose first key is `first`.
    int keyFor (const juce::var& node, int first, int index)
    {
        if (! (bool) node["whiteNoteSwitch"])
            return first + index;
        auto isWhite = [] (int pitch) {
            const int pc = ((pitch % 12) + 12) % 12;
            return pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11;
        };
        int key = first;
        while (! isWhite (key))
            ++key;
        for (int i = 0; i < index; ++i)
            do
                ++key;
            while (! isWhite (key));
        return key;
    }

    // Patches with velXFadeMode 1 follow velocity even when the preset
    // crossfades (VelXF sus: Staccato, Detache; timpani: hits).
    void patchModes (const juce::var& node, bool& anyVelocity, bool& anyCrossfade)
    {
        if (node["nodes"].size() > 0)
        {
            for (const auto& child : *node["nodes"].getArray())
                patchModes (child, anyVelocity, anyCrossfade);
            return;
        }
        if (! hasPatch (node))
            return;
        if ((int) node["parameters"]["velXFadeMode"] == 1)
            anyVelocity = true;
        else
            anyCrossfade = true;
    }

    using Layers = vslf::Articulation::Layers;

    // `crossfading`: the preset's velXFadeEnabled (nothing if the state lacks it).
    Layers layersOf (const juce::var& node, std::optional<bool> crossfading)
    {
        if (! crossfading)
            return Layers::Unknown;
        if (! *crossfading)
            return Layers::Velocity;
        bool anyVelocity = false, anyCrossfade = false;
        patchModes (node, anyVelocity, anyCrossfade);
        // A crossfade between patches (Marcato: sustain + staccato attack)
        // crossfades if any of them does.
        return anyCrossfade ? Layers::Crossfade : anyVelocity ? Layers::Velocity : Layers::Unknown;
    }

    constexpr int switchControlSlotFirst = 257; // nodeXFade0, Dim.Ctrl/A

    // A parallel dimension switched by a Dimension Control: which kind of
    // blend it is (from its slots' Mix/XFade), and where the control goes.
    void readBlend (const juce::var& node, const juce::var& mappings, SynchronArticulation& a)
    {
        using Blend = vslf::Articulation::Blend;
        const int control = (int) node["switchControl"];
        if (! (bool) node["parallel"] || control < switchControlSlotFirst || node["nodes"].size() < 2)
            return;
        const int slot = control - switchControlSlotFirst;

        int mixed = 0, faded = 0;
        for (const auto& child : *node["nodes"].getArray())
            ((int) child["parameters"]["nodeXFadeMode"] == 1 ? mixed : faded)++;
        if (faded == 0)
            return; // every slot always plays: the control does nothing

        const auto source = mappings[juce::Identifier ("nodeXFade" + juce::String (slot))]["source"].toString();
        int controller = -2;
        if (source == "velocity")
            controller = -1;
        else if (source.startsWith ("cc") && source.substring (2).containsOnly ("0123456789"))
            controller = source.substring (2).getIntValue();
        if (controller < -1 || controller > 127)
            return; // speed, aftertouch...: nothing the app writes

        a.blend.kind = mixed > 0 ? Blend::Kind::AddedLayer : Blend::Kind::Crossfade;
        a.blend.controller = controller;
        a.blend.name = node["subTitle"].toString().toStdString();
        a.blendSlot = slot;
    }

    void collect (const juce::var& node, juce::StringArray path, std::vector<int> keys,
                  std::optional<bool> crossfading, const juce::var& mappings, std::vector<SynchronArticulation>& out)
    {
        const auto first = firstKey (node);
        if (! first)
        {
            if (! path.isEmpty() && hasPatch (node))
            {
                SynchronArticulation a;
                a.path = path;
                a.keys = keys;
                a.layers = layersOf (node, crossfading);
                readBlend (node, mappings, a);
                out.push_back (a);
            }
            return;
        }
        const auto* children = node["nodes"].getArray();
        for (int i = 0; i < children->size(); ++i)
        {
            const auto& child = children->getReference (i);
            const int key = keyFor (node, *first, i);
            if (key > 127 || ! hasPatch (child))
                continue;
            auto childPath = path;
            childPath.add (child["title"].toString().trim());
            auto childKeys = keys;
            childKeys.push_back (key);
            collect (child, childPath, childKeys, crossfading, mappings, out);
        }
    }
}

SynchronArticulations synchronArticulationsFromJson (const juce::var& stateJson)
{
    SynchronArticulations result;
    const auto root = stateJson["data"]["custom"]["sampler"]["rootNode"];
    if (! root.isObject())
        return result;
    collect (root, {}, {}, crossfadingOf (stateJson), stateJson["data"]["custom"]["paramControl"]["mappings"], result.choices);
    for (int slot = 0; slot < 8; ++slot)
    {
        const auto v = stateJson["data"]["parameters"][juce::Identifier ("nodeXFade" + juce::String (slot))];
        if (! v.isVoid() && ! v.isUndefined())
            result.controlValues[slot] = juce::jlimit (0.0, 1.0, (double) v);
    }
    for (const char* name : { "attack", "release", "legatoBlur" })
    {
        const auto source = stateJson["data"]["custom"]["paramControl"]["mappings"][name]["source"].toString();
        const auto value = stateJson["data"]["parameters"][name];
        if (source.startsWith ("cc") && source.substring (2).containsOnly ("0123456789") && ! value.isVoid())
            result.performControls[name] = { source.substring (2).getIntValue(), juce::jlimit (0.0, 1.0, (double) value) };
    }

    // Follow the selection down the key-switched levels.
    auto node = root;
    while (const auto first = firstKey (node))
    {
        const int index = juce::roundToInt ((double) node["currentNodeIndex"]);
        if (! juce::isPositiveAndBelow (index, node["nodes"].size()))
            break;
        const int key = keyFor (node, *first, index);
        if (key > 127)
            break;
        result.currentKeys.push_back (key);
        node = node["nodes"][index];
    }
    return result;
}

std::optional<SynchronArticulations> synchronArticulations (const juce::MemoryBlock& state)
{
    const auto json = synchronStateJson (state);
    if (! json)
        return std::nullopt;
    const auto parsed = juce::JSON::parse (*json);
    if (! parsed["data"].isObject())
        return std::nullopt;
    return synchronArticulationsFromJson (parsed);
}

vslf::InstrumentProfile withTree (vslf::InstrumentProfile profile, const std::vector<SynchronArticulation>& tree,
                                  const std::vector<int>& startKeys, const std::map<int, double>& controlValues,
                                  const std::map<std::string, std::pair<int, double>>& performControls)
{
    auto perform = [&] (const char* name, vslf::InstrumentProfile::PerformControl& into) {
        if (const auto it = performControls.find (name); it != performControls.end())
            into = { it->second.first, it->second.second };
    };
    perform ("attack", profile.attackControl);
    perform ("release", profile.releaseControl);
    perform ("legatoBlur", profile.legatoBlurControl);

    if (tree.empty())
        return profile;

    std::map<int, int> levelOf;
    for (const auto& a : tree)
        for (size_t depth = 0; depth < a.keys.size(); ++depth)
            levelOf.emplace (a.keys[depth], treeLevelBase + (int) depth); // the first place a key appears wins

    auto& keys = profile.keys;
    keys.erase (std::remove_if (keys.begin(), keys.end(), [&] (const vslf::KeyswitchKey& k) { return levelOf.count (k.pitch) > 0; }),
                keys.end());
    for (const auto& [pitch, level] : levelOf)
        keys.push_back ({ pitch, level });

    auto sorted = [] (std::vector<int> v) { std::sort (v.begin(), v.end()); return v; };
    auto& mappings = profile.articulations;
    for (const auto& a : tree)
    {
        const auto k = sorted (a.keys);
        mappings.erase (std::remove_if (mappings.begin(), mappings.end(),
                                        [&] (const vslf::ArticulationMapping& m) { return sorted (m.keys) == k; }),
                        mappings.end());
        vslf::Articulation articulation { a.name().toStdString(), 0.0, a.layers, a.blend };
        if (const auto v = controlValues.find (a.blendSlot); v != controlValues.end())
            articulation.blend.centre = v->second;
        else if (a.blend.kind == vslf::Articulation::Blend::Kind::Crossfade)
            articulation.blend.centre = 0.0; // the first sound, until the preset says otherwise
        else
            articulation.blend.centre = 60.0 / 127.0; // VSL's default for an added layer
        mappings.push_back ({ a.keys, articulation });
        if (! startKeys.empty() && sorted (startKeys) == k)
            profile.defaultArticulation = articulation;
    }
    return profile;
}

std::vector<vslf::ArticulationMarking> resolvePicks (const std::vector<vslf::ArticulationMarking>& picks,
                                                     const std::vector<SynchronArticulation>& tree,
                                                     std::vector<std::string>& missing)
{
    if (tree.empty())
        return picks;
    std::vector<vslf::ArticulationMarking> out;
    for (auto pick : picks)
    {
        const auto it = std::find_if (tree.begin(), tree.end(), [&] (const SynchronArticulation& a) {
            return a.name().toStdString() == pick.name;
        });
        if (it == tree.end())
        {
            if (std::find (missing.begin(), missing.end(), pick.name) == missing.end())
                missing.push_back (pick.name);
            continue;
        }
        pick.keys = it->keys;
        out.push_back (pick);
    }
    return out;
}
}
