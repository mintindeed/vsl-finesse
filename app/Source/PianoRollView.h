#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppContext.h"

// Read-only view of the selected track: finessed notes (coloured by
// articulation, brightness = velocity), the original notes as outlines,
// phrases, keyswitches, markings, and the CC1 / CC11 curves. Drag across bars
// to select a passage; click (or drag in the bar ruler) to move the play
// position; scroll to move, pinch or Cmd/Ctrl+scroll to zoom.
class PianoRollView : public juce::Component,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    explicit PianoRollView (AppContext&);
    ~PianoRollView() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

    void zoomToFit();

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    static constexpr int headerHeight = 58;
    juce::Rectangle<int> headerArea() const;
    juce::Rectangle<int> markingsArea() const;
    int markingAt (const Track&, juce::Point<int>) const;
    void drawSelection (juce::Graphics&, const Track&);
    void drawMarkings (juce::Graphics&, const Track&);
    juce::Rectangle<int> notesArea() const;
    juce::Rectangle<int> dynamicsArea() const;
    juce::Rectangle<int> expressionArea() const;

    float xForSeconds (double s) const { return (float) ((s - viewStart) * pixelsPerSecond); }
    double secondsForX (float x) const { return viewStart + x / pixelsPerSecond; }

    void drawGrid (juce::Graphics&, const Track&);
    void drawNotes (juce::Graphics&, const Track&);
    void drawLane (juce::Graphics&, const Track&, juce::Rectangle<int> area, int cc, juce::Colour, const juce::String& label);
    void zoomAround (float x, double factor);

    AppContext& app;
    double pixelsPerSecond = 60.0;
    double viewStart = 0.0;
    float lastPlayheadX = -1.0f;
    float dragStartX = 0.0f;
    double dragStartView = 0.0;
    bool selecting = false;
    bool scrubbing = false;
};
