#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "AppContext.h"
#include "DynamicsBar.h"
#include "InspectorPanel.h"
#include "PianoRollView.h"
#include "TrackListView.h"
#include "WelcomeOverlay.h"

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::MenuBarModel,
                      private juce::Timer
{
public:
    explicit MainComponent (AppContext&);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // Opens files given on the command line / from Finder.
    void openFiles (const juce::StringArray& paths, int trackIndex = -1);

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int, const juce::String&) override;
    void menuItemSelected (int, int) override;

private:
    void timerCallback() override;
    void updateTransport();

    void chooseMidi (Track&);
    void exportSelected();
    void showAudioSettings();
    void saveSession (bool saveAs);
    void openSessionDialog();
    void showError (const juce::String& title, const juce::String& message);
    void showWelcome();
    void dismissWelcome();

    AppContext& app;
    juce::TooltipWindow tooltips { this, 600 };

    TrackListView trackList { app };
    DynamicsBar dynamicsBar { app };
    PianoRollView pianoRoll { app };
    InspectorPanel inspector { app };

    juce::TextButton playButton { "Play" }, startButton { "|<" };
    juce::TextButton originalButton { "Original" }, finessedButton { "Finessed" };
    juce::TextButton exportButton { "Export Finessed MIDI..." }, audioButton { "Audio..." }, fitButton { "Fit" };
    juce::Label positionLabel;

    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<WelcomeOverlay> welcome;
    bool dragOver = false;
};
