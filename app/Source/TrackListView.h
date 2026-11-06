#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppContext.h"

#include <functional>

// One row per track: name, MIDI file, instrument plug-in, mute/solo.
class TrackListView : public juce::Component,
                      private juce::ChangeListener
{
public:
    explicit TrackListView (AppContext&);
    ~TrackListView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Index of the track row under a point in this component, or -1.
    int trackAt (juce::Point<int>) const;

    int preferredHeight() const;

    std::function<void (Track&)> onOpenMidi;
    std::function<void()> onLayoutChanged;

private:
    class Row;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuild();

    AppContext& app;
    juce::OwnedArray<Row> rows;
    juce::TextButton addButton { "+ Add track" };
};
