#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioEngine.h"
#include "ProfileLibrary.h"
#include "Session.h"

#include <deque>
#include <functional>
#include <map>

// Owns everything with application lifetime and implements the user actions,
// so the UI components stay thin.
class AppContext : public juce::ChangeListener,
                   private juce::Timer
{
public:
    AppContext();
    ~AppContext() override;

    juce::ApplicationProperties properties;
    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    ProfileLibrary profiles;
    Session session;
    AudioEngine engine { session };

    // --- MIDI -------------------------------------------------------------
    bool loadMidi (Track&, const juce::File&, juce::String& error);
    bool exportFinessed (Track&, const juce::File&, juce::String& error);
    static bool isMidiFile (const juce::File&);

    // What happens to controller data (CCs) already in a MIDI file when it's
    // opened: always remove it, never remove it, or ask. The app's own CC1
    // and CC11 replace the file's either way, where it writes them.
    enum class FileControllers
    {
        Always, // remove
        Never,  // keep
        Ask
    };
    FileControllers fileControllers();
    // Always and Never apply to the tracks already open, too.
    void setFileControllers (FileControllers);
    // Keeping the file's controllers: whether the app's own CC1 and CC11 still
    // replace the file's (the default). Applies to every open track, too.
    bool replaceFileDynamics();
    void setReplaceFileDynamics (bool);

    // --- Instrument profiles and settings ----------------------------------
    void setProfile (Track&, const std::string& id);
    // Applies an edited profile to the track, saves it as the user's copy and
    // re-runs the engine.
    void updateProfile (Track&, const vslf::InstrumentProfile&);
    void setSettings (Track&, const vslf::Settings&);

    // Dynamics markings: applied to the selection, or the whole part if
    // nothing is selected. Clearing works the same way.
    void applyMarking (Track&, vslf::Marking);
    void clearMarkings (Track&);
    // Picked articulations work the same way: the selection, or the whole part.
    void applyArticulation (Track&, vslf::ArticulationMarking);
    void clearArticulations (Track&);
    static vslf::Tick partEnd (const Track&);
    void saveDefaultSettings (const vslf::Settings&);
    vslf::Settings defaultSettings();
    // The preset type last chosen becomes the default for new tracks.
    void rememberPresetType (vslf::PresetType);

    // --- Plugins ------------------------------------------------------------
    // Installed instruments. On macOS this lists Audio Unit instruments
    // without loading them; elsewhere it uses the scanned plug-in list.
    juce::Array<juce::PluginDescription> instrumentPlugins() const;
    static bool isViennaInstrument (const juce::PluginDescription&);
    static bool isSynchronPlayer (const juce::PluginDescription&);
    bool isSynchronInstalled() const;
    // The welcome overlay has told the user Synchron Player is missing, so the
    // pop-up saying the same is skipped.
    void synchronMissingExplained() { warnedSynchronMissing = true; }
    void showPluginScanner (juce::Component* parent);
    void loadPlugin (Track&, const juce::PluginDescription&, std::function<void (juce::String error)> done,
                     juce::MemoryBlock state = {});
    // Loads Synchron Player into a track that has no instrument yet, if it's
    // installed. Does nothing otherwise.
    void loadDefaultInstrument (Track&);
    void removePlugin (Track&);
    // If Synchron Player has loaded a different preset, sets the track's
    // instrument profile and preset folder to match it.
    void followSynchronPreset (Track&);
    void showPluginWindow (Track&);

    // Writes everything the instrument plug-in reports about itself (state,
    // parameters, programs, readable text in its state) to a JSON file on the
    // Desktop. Used to learn how Synchron Player stores its articulation tree.
    juce::File saveInstrumentDetails (Track&, juce::String& error);

    // --- Sessions -------------------------------------------------------------
    bool saveSession (const juce::File&, juce::String& error);
    void openSession (const juce::File&, std::function<void (juce::String errors)> done);
    juce::File currentSessionFile;

    void removeTrack (int index);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Applies the FileControllers setting to a track whose MIDI file is new.
    void decideControllers (Track&);
    void askNextControllerQuestion();
    std::deque<Track*> controllerQuestions;
    bool askingControllers = false;
    void timerCallback() override;
    void closePluginWindow (Track*);
    void destroyPlugin (std::unique_ptr<juce::AudioPluginInstance>);

    // Warnings that the app was built for Synchron Player, each shown once
    // per run.
    void warnSynchronMissing();
    void warnOtherInstrument (const juce::String& name);
    bool warnedSynchronMissing = false, warnedOtherInstrument = false;

    class PluginWindow;
    std::map<Track*, std::unique_ptr<PluginWindow>> pluginWindows;
};
