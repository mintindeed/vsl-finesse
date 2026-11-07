#include "SynchronState.h"

#include <zstd.h>

#include <algorithm>
#include <cstring>
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
}
