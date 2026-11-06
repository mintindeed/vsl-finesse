#include "PlaybackSequence.h"

#include <MidiIO.h>

#include <algorithm>
#include <map>
#include <set>

std::shared_ptr<const PlaybackSequence> PlaybackSequence::build (const vslp::Part& part,
                                                                 const vslp::InstrumentProfile& profile,
                                                                 int defaultDynamics)
{
    auto seq = std::make_shared<PlaybackSequence>();
    seq->tempoMap = part.tempo;
    seq->meters = part.timeSignatures;
    seq->dynamicsCC = profile.dynamicsCC;
    seq->expressionCC = profile.expressionCC;
    seq->defaultDynamicsValue = defaultDynamics;

    // Reuse the file writer's event ordering (note-offs, CCs, keyswitches, notes).
    const auto file = vslp::io::toMidiFile (part);
    std::set<int> channels;

    for (int t = 0; t < file.getNumTracks(); ++t)
    {
        const auto& track = *file.getTrack (t);
        for (int i = 0; i < track.getNumEvents(); ++i)
        {
            const auto& m = track.getEventPointer (i)->message;
            if (m.isMetaEvent() || m.isSysEx())
                continue;

            int level = -1;
            if ((m.isNoteOn() || m.isNoteOff()) && profile.isKeyswitch (m.getNoteNumber()))
                level = profile.levelOf (m.getNoteNumber());

            const double seconds = part.tempo.secondsAt (m.getTimeStamp());
            seq->events.push_back ({ seconds, m, level });
            seq->length = std::max (seq->length, seconds);
            channels.insert (m.getChannel());
        }
    }

    std::stable_sort (seq->events.begin(), seq->events.end(),
                      [] (const Event& a, const Event& b) { return a.seconds < b.seconds; });

    seq->usedChannels.assign (channels.begin(), channels.end());
    if (seq->usedChannels.empty())
        seq->usedChannels.push_back (1);
    return seq;
}

void PlaybackSequence::collect (double from, double to, double sampleRate, int numSamples, juce::MidiBuffer& out) const
{
    auto it = std::lower_bound (events.begin(), events.end(), from,
                                [] (const Event& e, double t) { return e.seconds < t; });
    for (; it != events.end() && it->seconds < to; ++it)
    {
        const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), (int) ((it->seconds - from) * sampleRate));
        out.addEvent (it->message, offset);
    }
}

void PlaybackSequence::chase (double t, juce::MidiBuffer& out) const
{
    std::map<std::pair<int, int>, int> controllers; // (channel, cc) -> value
    std::map<int, int> programs;                    // channel -> program
    std::map<int, int> pitchBends;                  // channel -> value
    std::map<int, const juce::MidiMessage*> keyForLevel;
    std::vector<int> levelOrder;

    for (const auto& e : events)
    {
        if (e.seconds >= t)
            break;
        const auto& m = e.message;
        if (m.isController())
            controllers[{ m.getChannel(), m.getControllerNumber() }] = m.getControllerValue();
        else if (m.isProgramChange())
            programs[m.getChannel()] = m.getProgramChangeNumber();
        else if (m.isPitchWheel())
            pitchBends[m.getChannel()] = m.getPitchWheelValue();
        else if (e.keyswitchLevel >= 0 && m.isNoteOn())
        {
            keyForLevel[e.keyswitchLevel] = &m;
            levelOrder.erase (std::remove (levelOrder.begin(), levelOrder.end(), e.keyswitchLevel), levelOrder.end());
            levelOrder.push_back (e.keyswitchLevel);
        }
    }

    for (int ch : usedChannels)
    {
        if (controllers.count ({ ch, dynamicsCC }) == 0)
            controllers[{ ch, dynamicsCC }] = defaultDynamicsValue;
        if (expressionCC >= 0 && controllers.count ({ ch, expressionCC }) == 0)
            controllers[{ ch, expressionCC }] = 127;
    }

    for (const auto& [ch, program] : programs)
        out.addEvent (juce::MidiMessage::programChange (ch, program), 0);
    for (const auto& [key, value] : controllers)
        out.addEvent (juce::MidiMessage::controllerEvent (key.first, key.second, value), 0);
    for (const auto& [ch, value] : pitchBends)
        out.addEvent (juce::MidiMessage::pitchWheel (ch, value), 0);

    // Replay the latest key of each level, oldest first, as quick taps.
    for (int level : levelOrder)
    {
        const auto& m = *keyForLevel[level];
        out.addEvent (juce::MidiMessage::noteOn (m.getChannel(), m.getNoteNumber(), m.getVelocity()), 0);
        out.addEvent (juce::MidiMessage::noteOff (m.getChannel(), m.getNoteNumber()), 0);
    }
}
