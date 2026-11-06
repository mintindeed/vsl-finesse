#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppContext.h"

// Right-hand panel for the selected track:
//  - Instrument: profile and playable range (notes outside it are keyswitches)
//  - Articulations: each keyswitch combination found in the file, with its
//    patch type (Velocity / VelXF sus - MOD / VelXF - MOD) and legato flag
//  - Performance: the shaping controls
//  - Report: what the engine did
class InspectorPanel : public juce::Component,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    explicit InspectorPanel (AppContext&);
    ~InspectorPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Content;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    AppContext& app;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
};
