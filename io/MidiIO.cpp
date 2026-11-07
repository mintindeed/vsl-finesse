#include "MidiIO.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace vslf::io
{

namespace
{

RawEvent rawFrom (const juce::MidiMessage& m, Tick tick)
{
    RawEvent e;
    e.tick = tick;
    const auto* data = m.getRawData();
    e.bytes.assign (data, data + m.getRawDataSize());
    return e;
}

juce::MidiMessage messageFrom (const RawEvent& e)
{
    juce::MidiMessage m (e.bytes.data(), (int) e.bytes.size(), double (e.tick));
    return m;
}

} // namespace

ReadResult readMidi (juce::InputStream& stream, const std::string& fallbackName)
{
    ReadResult result;
    juce::MidiFile file;
    int fileType = 0;

    if (! file.readFrom (stream, true, &fileType))
    {
        result.error = "Not a readable standard MIDI file.";
        return result;
    }

    const int ppq = file.getTimeFormat();
    if (ppq <= 0)
    {
        result.error = "SMPTE-timed MIDI files aren't supported - export with musical (bars/beats) timing.";
        return result;
    }

    Part& part = result.part;
    part.ppq = ppq;

    std::vector<TempoChange> tempo;
    std::string firstTrackName, noteTrackName;
    std::set<int> tracksWithNotes;

    for (int t = 0; t < file.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence seq (*file.getTrack (t));
        seq.updateMatchedPairs();

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto* holder = seq.getEventPointer (i);
            const auto& m = holder->message;
            const Tick tick = (Tick) std::llround (m.getTimeStamp());

            if (m.isMetaEvent())
            {
                if (m.isTempoMetaEvent())
                    tempo.push_back ({ tick, m.getTempoSecondsPerQuarterNote() * 1.0e6 });
                else if (m.isTimeSignatureMetaEvent())
                {
                    int num = 4, den = 4;
                    m.getTimeSignatureInfo (num, den);
                    part.timeSignatures.push_back ({ tick, num, den });
                }
                else if (m.isTrackNameEvent())
                {
                    const auto name = m.getTextFromTextMetaEvent().toStdString();
                    if (firstTrackName.empty())
                        firstTrackName = name;
                    if (noteTrackName.empty() && seq.getNumEvents() > 0)
                    {
                        // Remember the name of a track that actually contains notes.
                        for (int k = 0; k < seq.getNumEvents(); ++k)
                            if (seq.getEventPointer (k)->message.isNoteOn())
                            {
                                noteTrackName = name;
                                break;
                            }
                    }
                }
                else if (! m.isEndOfTrackMetaEvent())
                    part.metaEvents.push_back (rawFrom (m, tick));
                continue;
            }

            if (m.isNoteOn())
            {
                Note n;
                n.start = tick;
                n.pitch = m.getNoteNumber();
                n.velocity = m.getVelocity();
                n.channel = m.getChannel();
                n.end = holder->noteOffObject != nullptr
                            ? (Tick) std::llround (holder->noteOffObject->message.getTimeStamp())
                            : tick + ppq;
                if (n.end > n.start)
                    part.notes.push_back (n);
                tracksWithNotes.insert (t);
                continue;
            }

            if (m.isNoteOff())
                continue;

            if (m.isController())
            {
                part.controllers.push_back ({ tick, m.getControllerNumber(), m.getControllerValue(), m.getChannel() });
                continue;
            }

            part.channelEvents.push_back (rawFrom (m, tick));
        }
    }

    std::stable_sort (part.notes.begin(), part.notes.end(),
                      [] (const Note& a, const Note& b) { return a.start < b.start; });
    std::stable_sort (part.timeSignatures.begin(), part.timeSignatures.end(),
                      [] (const TimeSignature& a, const TimeSignature& b) { return a.tick < b.tick; });

    part.tempo = TempoMap (ppq, tempo);
    part.name = ! noteTrackName.empty() ? noteTrackName : ! firstTrackName.empty() ? firstTrackName : fallbackName;

    if (tracksWithNotes.size() > 1)
        result.warnings.push_back ("The file has " + std::to_string (tracksWithNotes.size())
                                   + " tracks with notes - they were merged into one part.");

    result.ok = true;
    return result;
}

ReadResult readMidiFile (const juce::File& file)
{
    juce::FileInputStream stream (file);
    if (! stream.openedOk())
    {
        ReadResult r;
        r.error = "Couldn't open " + file.getFullPathName().toStdString();
        return r;
    }
    return readMidi (stream, file.getFileNameWithoutExtension().toStdString());
}

juce::MidiFile toMidiFile (const Part& part)
{
    juce::MidiFile file;
    file.setTicksPerQuarterNote (part.ppq);

    juce::MidiMessageSequence conductor;
    for (const auto& t : part.tempo.changes())
    {
        auto m = juce::MidiMessage::tempoMetaEvent ((int) std::lround (t.microsecondsPerQuarter));
        m.setTimeStamp (double (t.tick));
        conductor.addEvent (m);
    }
    for (const auto& ts : part.timeSignatures)
    {
        auto m = juce::MidiMessage::timeSignatureMetaEvent (ts.numerator, ts.denominator);
        m.setTimeStamp (double (ts.tick));
        conductor.addEvent (m);
    }
    for (const auto& e : part.metaEvents)
        conductor.addEvent (messageFrom (e));
    conductor.sort();

    // Events at the same tick keep the order they're added in (stable sort),
    // so add them in the order an instrument needs them:
    // note-offs, controllers, other channel events, keyswitches, then notes.
    juce::MidiMessageSequence track;
    {
        auto name = juce::MidiMessage::textMetaEvent (3, juce::String (part.name));
        name.setTimeStamp (0.0);
        track.addEvent (name);
    }

    auto addOffs = [&track] (const std::vector<Note>& notes) {
        for (const auto& n : notes)
        {
            auto off = juce::MidiMessage::noteOff (n.channel, n.pitch);
            off.setTimeStamp (double (n.end));
            track.addEvent (off);
        }
    };
    auto addOns = [&track] (const std::vector<Note>& notes) {
        for (const auto& n : notes)
        {
            auto on = juce::MidiMessage::noteOn (n.channel, n.pitch, (juce::uint8) std::clamp (n.velocity, 1, 127));
            on.setTimeStamp (double (n.start));
            track.addEvent (on);
        }
    };

    addOffs (part.notes);
    addOffs (part.keyswitches);
    for (const auto& c : part.controllers)
    {
        auto m = juce::MidiMessage::controllerEvent (c.channel, c.controller, std::clamp (c.value, 0, 127));
        m.setTimeStamp (double (c.tick));
        track.addEvent (m);
    }
    for (const auto& e : part.channelEvents)
        track.addEvent (messageFrom (e));
    addOns (part.keyswitches);
    addOns (part.notes);

    track.sort();
    track.updateMatchedPairs();

    file.addTrack (conductor);
    file.addTrack (track);
    return file;
}

bool writeMidiFile (const Part& part, const juce::File& file, std::string& error)
{
    const auto midi = toMidiFile (part);

    juce::TemporaryFile temp (file);
    {
        juce::FileOutputStream out (temp.getFile());
        if (! out.openedOk())
        {
            error = "Couldn't write to " + file.getFullPathName().toStdString();
            return false;
        }
        if (! midi.writeTo (out, 1))
        {
            error = "Writing the MIDI data failed.";
            return false;
        }
    }

    if (! temp.overwriteTargetFileWithTemporary())
    {
        error = "Couldn't replace " + file.getFullPathName().toStdString();
        return false;
    }
    return true;
}

juce::File finessedFileFor (const juce::File& original)
{
    return original.getSiblingFile (original.getFileNameWithoutExtension() + " (Finessed).mid");
}

} // namespace vslf::io
