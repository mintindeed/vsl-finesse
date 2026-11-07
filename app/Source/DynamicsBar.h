#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppContext.h"

// The strip above the piano roll: pp..ff, crescendo, diminuendo and clear,
// applied to the selected bars (drag in the piano roll) or the whole part.
class DynamicsBar : public juce::Component,
                    private juce::ChangeListener
{
public:
    explicit DynamicsBar (AppContext&);
    ~DynamicsBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    void apply (vslf::Marking::Kind, int level);

    AppContext& app;
    juce::Label scopeLabel;
    juce::OwnedArray<juce::TextButton> dynamicButtons;
    juce::TextButton crescButton { "cresc." }, dimButton { "dim." }, clearButton { "Clear" };
};
