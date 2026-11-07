#include "Session.h"

#include <algorithm>

int Track::flatDynamics() const
{
    std::vector<int> v;
    for (const auto& n : input.notes)
        if (! profile.isKeyswitch (n.pitch))
            v.push_back (n.velocity);
    if (v.empty())
        return 100;
    std::nth_element (v.begin(), v.begin() + long (v.size() / 2), v.end());
    return v[v.size() / 2];
}

vslf::InstrumentProfile Track::engineProfile() const
{
    return vslf::io::withTree (profile, synchronArticulations, articulationStartKeys);
}

bool Track::isPickMissing (const vslf::ArticulationMarking& m) const
{
    return ! synchronArticulations.empty()
           && std::none_of (synchronArticulations.begin(), synchronArticulations.end(),
                            [&] (const vslf::io::SynchronArticulation& a) { return a.name().toStdString() == m.name; });
}

Session::Session()
{
    addTrack();
}

Track* Session::track (int index) const
{
    return juce::isPositiveAndBelow (index, (int) trackList.size()) ? trackList[(size_t) index].get() : nullptr;
}

Track& Session::addTrack()
{
    auto t = std::make_unique<Track> ("Track " + juce::String (trackList.size() + 1));
    auto& ref = *t;
    {
        const juce::ScopedLock sl (audioLock);
        trackList.push_back (std::move (t));
    }
    selected = (int) trackList.size() - 1;
    sendChangeMessage();
    return ref;
}

std::unique_ptr<juce::AudioPluginInstance> Session::removeTrack (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) trackList.size()))
        return {};

    std::unique_ptr<Track> removed;
    {
        const juce::ScopedLock sl (audioLock);
        removed = std::move (trackList[(size_t) index]);
        trackList.erase (trackList.begin() + index);
    }
    selected = juce::jlimit (0, juce::jmax (0, (int) trackList.size() - 1), selected);
    sendChangeMessage();
    return std::move (removed->audio.plugin);
}

void Session::setSelection (vslf::Tick start, vslf::Tick end)
{
    if (end < start)
        std::swap (start, end);
    if (start == selectionStart && end == selectionEnd)
        return;
    selectionStart = start;
    selectionEnd = end;
    sendChangeMessage();
}

void Session::clearSelection()
{
    setSelection (0, 0);
}

void Session::select (int index)
{
    if (index != selected && track (index) != nullptr)
    {
        selected = index;
        selectionStart = selectionEnd = 0;
        sendChangeMessage();
    }
}

int Session::indexOf (const Track* t) const
{
    for (size_t i = 0; i < trackList.size(); ++i)
        if (trackList[i].get() == t)
            return (int) i;
    return -1;
}

void Session::process (Track& t)
{
    if (! t.hasMidi)
        return;

    // The loaded preset's tree says which level each key is on and what it's
    // called. Picks are looked up again by name in it.
    const auto profile = t.engineProfile();
    std::vector<std::string> missing;
    vslf::ArticulationPicks picks;
    picks.markings = vslf::io::resolvePicks (t.articulationMarkings, t.synchronArticulations, missing);
    picks.startKeys = t.articulationStartKeys;

    // Without a tree, the picks name their own key combinations.
    auto named = profile;
    if (t.synchronArticulations.empty())
        for (const auto& m : picks.markings)
            if (const auto* found = named.findArticulation (m.keys); found == nullptr || found->keys.size() != m.keys.size())
                named.articulations.push_back ({ m.keys, { m.name, 0.0 } });

    t.result = vslf::finesse (t.input, named, t.settings, t.markings, picks);
    for (const auto& name : missing)
        t.result.report.warnings.push_back ("\"" + name + "\" isn't in the loaded Synchron preset, so no keyswitches are written for it. "
                                            "Pick another articulation for that passage, or remove it.");

    auto original = PlaybackSequence::build (t.input, profile, t.flatDynamics());
    auto finessed = PlaybackSequence::build (t.result.part, profile, t.flatDynamics());

    {
        const juce::ScopedLock sl (audioLock);
        std::swap (t.audio.original, original);
        std::swap (t.audio.finessed, finessed);
    }
    // The previous sequences are released here, on the message thread.
    sendChangeMessage();
}

std::unique_ptr<juce::AudioPluginInstance> Session::swapPlugin (Track& t, std::unique_ptr<juce::AudioPluginInstance> plugin)
{
    {
        const juce::ScopedLock sl (audioLock);
        std::swap (t.audio.plugin, plugin);
        for (auto& ch : t.audio.sounding)
            ch.reset();
    }
    sendChangeMessage();
    return plugin;
}

double Session::lengthSeconds() const
{
    double length = 0.0;
    for (const auto& t : trackList)
    {
        if (t->audio.finessed != nullptr)
            length = std::max (length, t->audio.finessed->lengthSeconds());
        if (t->audio.original != nullptr)
            length = std::max (length, t->audio.original->lengthSeconds());
    }
    return length;
}

std::vector<std::unique_ptr<juce::AudioPluginInstance>> Session::clear()
{
    std::vector<std::unique_ptr<Track>> removed;
    {
        const juce::ScopedLock sl (audioLock);
        removed = std::move (trackList);
        trackList.clear();
    }
    selected = 0;

    std::vector<std::unique_ptr<juce::AudioPluginInstance>> plugins;
    for (auto& t : removed)
        if (t->audio.plugin != nullptr)
            plugins.push_back (std::move (t->audio.plugin));
    sendChangeMessage();
    return plugins;
}
