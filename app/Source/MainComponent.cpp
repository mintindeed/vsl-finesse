#include "MainComponent.h"
#include "Theme.h"

#include <MidiIO.h>

namespace
{
constexpr int toolbarHeight = 48;
constexpr int inspectorWidth = 380;

enum MenuIds : int
{
    menuOpenMidi = 1,
    menuExportMidi,
    menuOpenSession,
    menuSaveSession,
    menuSaveSessionAs,
    menuAudioSettings,
    menuScanPlugins,
    menuProfilesFolder,
    menuQuit,
    menuWelcome,
    menuSettings
};

// --- Settings ---------------------------------------------------------------
class SettingsContent : public juce::Component
{
public:
    explicit SettingsContent (AppContext& a) : app (a)
    {
        heading.setText ("Controllers already in MIDI files", juce::dontSendNotification);
        heading.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        intro.setText ("Controller data a file brings with it, such as CC20 (Synchron's tremolo crossfade) or "
                       "CC64 (sustain pedal). Remove it:", juce::dontSendNotification);
        note.setText ("Either way, VSL Finesse's own dynamics (CC1) and expression (CC11) replace the file's where it "
                      "writes them. Always and Never also apply to the tracks already open.", juce::dontSendNotification);
        note.setColour (juce::Label::textColourId, theme::textDim);
        for (auto* l : { &heading, &intro, &note })
        {
            l->setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (l);
        }
        intro.setFont (juce::FontOptions (13.0f));
        note.setFont (juce::FontOptions (12.0f));

        const AppContext::FileControllers choices[] = { AppContext::FileControllers::Always, AppContext::FileControllers::Never,
                                                        AppContext::FileControllers::Ask };
        const char* labels[] = { "Always", "Never", "Ask when a file has some" };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = buttons.add (new juce::ToggleButton (labels[i]));
            b->setRadioGroupId (1);
            b->setToggleState (app.fileControllers() == choices[i], juce::dontSendNotification);
            b->onClick = [this, b, choice = choices[i]] {
                if (b->getToggleState())
                    app.setFileControllers (choice);
            };
            addAndMakeVisible (b);
        }
        setSize (460, 230);
    }

    void paint (juce::Graphics& g) override { g.fillAll (theme::panel); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 14);
        heading.setBounds (r.removeFromTop (24));
        intro.setBounds (r.removeFromTop (38));
        for (auto* b : buttons)
            b->setBounds (r.removeFromTop (26).withTrimmedLeft (4));
        r.removeFromTop (8);
        note.setBounds (r);
    }

private:
    AppContext& app;
    juce::Label heading, intro, note;
    juce::OwnedArray<juce::ToggleButton> buttons;
};

// Bump to show the welcome overlay again after its content changes.
constexpr int welcomeVersion = 1;

juce::String formatTime (double seconds)
{
    const int total = (int) seconds;
    return juce::String::formatted ("%d:%02d.%d", total / 60, total % 60, (int) ((seconds - total) * 10.0));
}
} // namespace

MainComponent::MainComponent (AppContext& a) : app (a)
{
    setWantsKeyboardFocus (true);

    addAndMakeVisible (trackList);
    addAndMakeVisible (dynamicsBar);
    addAndMakeVisible (pianoRoll);
    addAndMakeVisible (inspector);

    trackList.onOpenMidi = [this] (Track& t) { chooseMidi (t); };
    trackList.onLayoutChanged = [this] { resized(); };

    for (auto* b : { &playButton, &startButton, &originalButton, &finessedButton, &exportButton, &audioButton, &fitButton })
        addAndMakeVisible (b);

    playButton.setTooltip ("Play / stop (space)");
    playButton.onClick = [this] { app.engine.togglePlay(); updateTransport(); };
    startButton.setTooltip ("Back to the start (return)");
    startButton.onClick = [this] { app.engine.setPosition (0.0); };

    originalButton.setTooltip ("Hear the file as it came in (T toggles)");
    finessedButton.setTooltip ("Hear the finessed performance (T toggles)");
    for (auto* b : { &originalButton, &finessedButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->setColour (juce::TextButton::buttonOnColourId, theme::accent.darker (0.3f));
    }
    originalButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    finessedButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    finessedButton.setToggleState (true, juce::dontSendNotification);
    originalButton.onClick = [this] { app.engine.setPlayOriginal (originalButton.getToggleState()); };
    finessedButton.onClick = [this] { app.engine.setPlayOriginal (! finessedButton.getToggleState()); };

    exportButton.setTooltip ("Save the selected track's finessed MIDI");
    exportButton.onClick = [this] { exportSelected(); };
    audioButton.setTooltip ("Audio output settings");
    audioButton.onClick = [this] { showAudioSettings(); };
    fitButton.setTooltip ("Zoom to fit the whole part");
    fitButton.onClick = [this] { pianoRoll.zoomToFit(); };

    addAndMakeVisible (positionLabel);
    positionLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain));
    positionLabel.setColour (juce::Label::textColourId, theme::text);
    positionLabel.setJustificationType (juce::Justification::centred);

   #if JUCE_MAC
    juce::PopupMenu appMenu;
    appMenu.addItem (menuSettings, "Settings...");
    juce::MenuBarModel::setMacMainMenu (this, &appMenu);
   #endif

    setSize (1320, 820);
    startTimerHz (15);

    if (app.properties.getUserSettings()->getIntValue ("welcomeShown") < welcomeVersion)
        showWelcome();
}

void MainComponent::showWelcome()
{
    if (welcome != nullptr)
        return;
    const bool installed = app.isSynchronInstalled();
    if (! installed)
        app.synchronMissingExplained();
    welcome = std::make_unique<WelcomeOverlay> (installed);
    welcome->onDismiss = [this] {
        // Deferred: this is called from the overlay's own button.
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::MessageManager::callAsync ([safe] {
            if (safe != nullptr)
                safe->dismissWelcome();
        });
    };
    addAndMakeVisible (*welcome);
    welcome->setBounds (getLocalBounds());
    juce::Component::SafePointer<WelcomeOverlay> overlay (welcome.get());
    juce::MessageManager::callAsync ([overlay] {
        if (overlay != nullptr && overlay->isShowing())
            overlay->grabKeyboardFocus();
    });
}

void MainComponent::dismissWelcome()
{
    welcome.reset();
    app.properties.getUserSettings()->setValue ("welcomeShown", welcomeVersion);
    app.properties.saveIfNeeded();
    grabKeyboardFocus();
}

MainComponent::~MainComponent()
{
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    g.setColour (theme::panel);
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));
    g.setColour (theme::outline);
    g.drawHorizontalLine (toolbarHeight - 1, 0.0f, (float) getWidth());
}

void MainComponent::paintOverChildren (juce::Graphics& g)
{
    if (dragOver)
    {
        g.setColour (theme::accent.withAlpha (0.15f));
        g.fillRect (getLocalBounds());
        g.setColour (theme::accent);
        g.drawRect (getLocalBounds(), 3);
    }
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop (toolbarHeight).reduced (10, 10);

    startButton.setBounds (bar.removeFromLeft (36));
    bar.removeFromLeft (4);
    playButton.setBounds (bar.removeFromLeft (70));
    bar.removeFromLeft (10);
    positionLabel.setBounds (bar.removeFromLeft (90));
    bar.removeFromLeft (16);
    originalButton.setBounds (bar.removeFromLeft (84));
    finessedButton.setBounds (bar.removeFromLeft (84));
    bar.removeFromLeft (10);
    fitButton.setBounds (bar.removeFromLeft (44));

    audioButton.setBounds (bar.removeFromRight (70));
    bar.removeFromRight (8);
    exportButton.setBounds (bar.removeFromRight (190));

    inspector.setBounds (r.removeFromRight (inspectorWidth));
    trackList.setBounds (r.removeFromTop (juce::jmin (trackList.preferredHeight(), r.getHeight() / 3)));
    dynamicsBar.setBounds (r.removeFromTop (40));
    pianoRoll.setBounds (r);

    if (welcome != nullptr)
        welcome->setBounds (getLocalBounds());
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        app.engine.togglePlay();
        updateTransport();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        app.session.clearSelection();
        return true;
    }
    if (key == juce::KeyPress (',', juce::ModifierKeys::commandModifier, 0))
    {
        showSettings();
        return true;
    }
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::homeKey)
    {
        app.engine.setPosition (0.0);
        return true;
    }
    if (key.getTextCharacter() == 't' || key.getTextCharacter() == 'T')
    {
        const bool original = ! app.engine.isPlayingOriginal();
        app.engine.setPlayOriginal (original);
        (original ? originalButton : finessedButton).setToggleState (true, juce::dontSendNotification);
        return true;
    }
    return false;
}

void MainComponent::timerCallback() { updateTransport(); }

void MainComponent::updateTransport()
{
    playButton.setButtonText (app.engine.isPlaying() ? "Stop" : "Play");
    positionLabel.setText (formatTime (app.engine.getPositionSeconds()), juce::dontSendNotification);
    auto* t = app.session.selectedTrack();
    exportButton.setEnabled (t != nullptr && t->hasMidi);
}

// --- Files -----------------------------------------------------------------------

void MainComponent::openFiles (const juce::StringArray& paths, int trackIndex)
{
    juce::StringArray errors;
    bool first = true;

    for (const auto& path : paths)
    {
        const juce::File f (path);
        if (f.hasFileExtension ("vslfsession;vslpsession")) // .vslpsession: saved before the rename to VSL Finesse
        {
            app.openSession (f, [this] (juce::String e) {
                if (e.isNotEmpty())
                    showError ("Some of the session couldn't be restored", e);
                pianoRoll.zoomToFit();
            });
            return;
        }
        if (! AppContext::isMidiFile (f))
            continue;

        // The first file goes to the target track (or the selected one);
        // further files fill empty tracks, then new ones.
        Track* t = nullptr;
        if (first)
            t = app.session.track (trackIndex >= 0 ? trackIndex : app.session.selectedIndex());
        else
            for (const auto& tr : app.session.tracks())
                if (! tr->hasMidi)
                {
                    t = tr.get();
                    break;
                }
        if (t == nullptr)
        {
            t = &app.session.addTrack();
            t->profile = app.profiles.fallback();
            t->settings = app.defaultSettings();
        }
        first = false;

        juce::String error;
        if (! app.loadMidi (*t, f, error))
            errors.add (f.getFileName() + ": " + error);
        else
        {
            app.session.select (app.session.indexOf (t));
            app.loadDefaultInstrument (*t);
        }
    }

    if (! errors.isEmpty())
        showError ("Couldn't open MIDI", errors.joinIntoString ("\n"));
    pianoRoll.zoomToFit();
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (AppContext::isMidiFile (juce::File (f)) || juce::File (f).hasFileExtension ("vslfsession;vslpsession"))
            return true;
    return false;
}

void MainComponent::fileDragEnter (const juce::StringArray&, int, int)
{
    dragOver = true;
    repaint();
}

void MainComponent::fileDragExit (const juce::StringArray&)
{
    dragOver = false;
    repaint();
}

void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    dragOver = false;
    repaint();
    const auto inList = trackList.getLocalPoint (this, juce::Point<int> (x, y));
    const int target = trackList.getLocalBounds().contains (inList) ? trackList.trackAt (inList) : -1;
    openFiles (files, target);
}

void MainComponent::chooseMidi (Track& t)
{
    const int index = app.session.indexOf (&t);
    chooser = std::make_unique<juce::FileChooser> ("Open a MIDI file (one instrument)", t.midiFile, "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, index] (const juce::FileChooser& fc) {
                              if (fc.getResult() != juce::File())
                                  openFiles ({ fc.getResult().getFullPathName() }, index);
                          });
}

void MainComponent::exportSelected()
{
    auto* t = app.session.selectedTrack();
    if (t == nullptr || ! t->hasMidi)
        return;

    const int index = app.session.selectedIndex();
    chooser = std::make_unique<juce::FileChooser> ("Export finessed MIDI", vslf::io::finessedFileFor (t->midiFile), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, index] (const juce::FileChooser& fc) {
                              auto file = fc.getResult();
                              auto* tr = app.session.track (index);
                              if (file == juce::File() || tr == nullptr)
                                  return;
                              if (! file.hasFileExtension ("mid;midi"))
                                  file = file.withFileExtension ("mid");
                              juce::String error;
                              if (! app.exportFinessed (*tr, file, error))
                                  showError ("Export failed", error);
                          });
}

void MainComponent::showSettings()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new SettingsContent (app));
    o.dialogTitle = "Settings";
    o.componentToCentreAround = this;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (app.deviceManager, 0, 0, 2, 2, false, false, true, false);
    selector->setSize (500, 400);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Audio settings";
    o.componentToCentreAround = this;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::saveSession (bool saveAs)
{
    auto doSave = [this] (const juce::File& file) {
        juce::String error;
        if (! app.saveSession (file, error))
            showError ("Couldn't save the session", error);
    };

    if (! saveAs && app.currentSessionFile != juce::File())
    {
        doSave (app.currentSessionFile);
        return;
    }

    auto* t = app.session.track (0);
    const auto start = t != nullptr && t->hasMidi ? t->midiFile.withFileExtension ("vslfsession")
                                                  : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Untitled.vslfsession");
    chooser = std::make_unique<juce::FileChooser> ("Save session", start, "*.vslfsession");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [doSave] (const juce::FileChooser& fc) {
                              if (fc.getResult() != juce::File())
                                  doSave (fc.getResult().withFileExtension ("vslfsession"));
                          });
}

void MainComponent::openSessionDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Open session", app.currentSessionFile, "*.vslfsession;*.vslpsession");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) {
                              if (fc.getResult() != juce::File())
                                  openFiles ({ fc.getResult().getFullPathName() });
                          });
}

void MainComponent::showError (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, message);
}

// --- Menus -------------------------------------------------------------------------

juce::StringArray MainComponent::getMenuBarNames() { return { "File", "Audio", "Help" }; }

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    if (index == 0)
    {
        auto* t = app.session.selectedTrack();
        m.addItem (menuOpenMidi, "Open MIDI...");
        m.addItem (menuExportMidi, "Export Finessed MIDI...", t != nullptr && t->hasMidi);
        m.addSeparator();
        m.addItem (menuOpenSession, "Open Session...");
        m.addItem (menuSaveSession, "Save Session");
        m.addItem (menuSaveSessionAs, "Save Session As...");
        m.addSeparator();
        m.addItem (menuProfilesFolder, "Show Instrument Profiles Folder");
       #if ! JUCE_MAC
        m.addItem (menuSettings, "Settings...");
        m.addSeparator();
        m.addItem (menuQuit, "Quit");
       #endif
    }
    else if (index == 1)
    {
        m.addItem (menuAudioSettings, "Audio Settings...");
       #if ! JUCE_MAC
        m.addItem (menuScanPlugins, "Scan for Plug-ins...");
       #endif
    }
    else if (index == 2)
    {
        m.addItem (menuWelcome, "Welcome and Permissions...");
    }
    return m;
}

void MainComponent::menuItemSelected (int id, int)
{
    switch (id)
    {
        case menuOpenMidi:
            if (auto* t = app.session.selectedTrack())
                chooseMidi (*t);
            break;
        case menuExportMidi:     exportSelected(); break;
        case menuOpenSession:    openSessionDialog(); break;
        case menuSaveSession:    this->saveSession (false); break;
        case menuSaveSessionAs:  this->saveSession (true); break;
        case menuAudioSettings:  showAudioSettings(); break;
        case menuScanPlugins:    app.showPluginScanner (this); break;
        case menuProfilesFolder:
            app.profiles.folder().createDirectory();
            app.profiles.folder().revealToUser();
            break;
        case menuWelcome:        showWelcome(); break;
        case menuSettings:       showSettings(); break;
        case menuQuit:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;
        default: break;
    }
}
