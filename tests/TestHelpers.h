#pragma once

#include <vslp/Engine.h>

#include <algorithm>
#include <vector>

namespace vslp::test
{

// A part at 120 bpm, 480 ppq, 4/4.
inline Part makePart (std::vector<Note> notes, std::vector<Note> keyswitches = {})
{
    Part p;
    p.name = "Test";
    p.ppq = 480;
    p.tempo = TempoMap (480, { { 0, 500000.0 } });
    p.timeSignatures = { { 0, 4, 4 } };
    for (auto& n : notes)
    {
        if (n.channel == 0) n.channel = 1;
    }
    p.notes = std::move (notes);
    p.notes.insert (p.notes.end(), keyswitches.begin(), keyswitches.end());
    return p;
}

inline Note note (Tick start, Tick length, int pitch, int velocity = 80)
{
    return Note { start, start + length, pitch, velocity, 1 };
}

// A simple scale-like melody: `count` notes of `length` ticks, no gaps.
inline std::vector<Note> line (int count, Tick length, int firstPitch = 67, Tick start = 0)
{
    std::vector<Note> v;
    static const int steps[] = { 0, 2, 4, 5, 7, 5, 4, 2 };
    for (int i = 0; i < count; ++i)
        v.push_back (note (start + i * length, length, firstPitch + steps[i % 8]));
    return v;
}

inline std::vector<ControllerEvent> controllersFor (const Part& p, int cc)
{
    std::vector<ControllerEvent> out;
    for (const auto& c : p.controllers)
        if (c.controller == cc)
            out.push_back (c);
    return out;
}

// Value of a controller at tick (last event at or before it), -1 if none.
inline int controllerAt (const Part& p, int cc, Tick tick)
{
    int v = -1;
    for (const auto& c : p.controllers)
        if (c.controller == cc && c.tick <= tick)
            v = c.value;
    return v;
}

inline InstrumentProfile violinProfile()
{
    InstrumentProfile p;
    p.id = "test-violins";
    p.family = Family::Strings;
    p.lowestNote = 55;
    p.highestNote = 103;
    return p;
}

} // namespace vslp::test
