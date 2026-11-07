#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// Shown over the main window the first time the app starts (and from
// Help > Welcome and Permissions): what the app is built for, and why macOS
// asks for the permissions it does.
class WelcomeOverlay : public juce::Component
{
public:
    explicit WelcomeOverlay (bool synchronInstalled);
    ~WelcomeOverlay() override;

    std::function<void()> onDismiss;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override {} // clicks don't reach the window behind

private:
    class Text;

    juce::Rectangle<int> cardBounds() const;

    juce::Viewport viewport;
    std::unique_ptr<Text> text;
    juce::TextButton okButton { "Got it" };
};
