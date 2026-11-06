#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioEngine.h"
#include "ProfileLibrary.h"
#include "Session.h"

#include <functional>
#include <map>

// Owns everything with application lifetime and implements the user actions,
// so the UI components stay thin.
class AppContext : public juce::ChangeListener
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

    // --- Instrument profiles and settings ----------------------------------
    void setProfile (Track&, const std::string& id);
    // Applies an edited profile to the track, saves it as the user's copy and
    // re-runs the engine.
    void updateProfile (Track&, const vslp::InstrumentProfile&);
    void setSettings (Track&, const vslp::Settings&);
    void saveDefaultSettings (const vslp::Settings&);
    vslp::Settings defaultSettings();

    // --- Plugins ------------------------------------------------------------
    // Installed instruments. On macOS this lists Audio Unit instruments
    // without loading them; elsewhere it uses the scanned plug-in list.
    juce::Array<juce::PluginDescription> instrumentPlugins() const;
    static bool isViennaInstrument (const juce::PluginDescription&);
    void showPluginScanner (juce::Component* parent);
    void loadPlugin (Track&, const juce::PluginDescription&, std::function<void (juce::String error)> done,
                     juce::MemoryBlock state = {});
    void removePlugin (Track&);
    void showPluginWindow (Track&);

    // --- Sessions -------------------------------------------------------------
    bool saveSession (const juce::File&, juce::String& error);
    void openSession (const juce::File&, std::function<void (juce::String errors)> done);
    juce::File currentSessionFile;

    void removeTrack (int index);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void closePluginWindow (Track*);
    void destroyPlugin (std::unique_ptr<juce::AudioPluginInstance>);

    class PluginWindow;
    std::map<Track*, std::unique_ptr<PluginWindow>> pluginWindows;
};
