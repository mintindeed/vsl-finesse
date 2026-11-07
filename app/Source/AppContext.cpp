#include "AppContext.h"

#include <JsonIO.h>
#include <MidiIO.h>
#include <SynchronState.h>

#include <algorithm>
#include <utility>

// ---------------------------------------------------------------------------
class AppContext::PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioProcessorEditor* editor, const juce::String& title, std::function<void()> onClose)
        : DocumentWindow (title, juce::Colours::darkgrey, DocumentWindow::closeButton | DocumentWindow::minimiseButton),
          closeCallback (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (editor, true);
        setResizable (editor->isResizable(), false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~PluginWindow() override { clearContentComponent(); }

    void closeButtonPressed() override
    {
        // Deferred, because closing deletes this window.
        if (closeCallback)
            juce::MessageManager::callAsync (closeCallback);
    }

private:
    std::function<void()> closeCallback;
};

// ---------------------------------------------------------------------------
AppContext::AppContext()
{
    ProfileLibrary::appDataFolder(); // moves a pre-rename "VSL Performer" folder before the settings are opened

    juce::PropertiesFile::Options options;
    options.applicationName = "VSL Finesse";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "VSL Finesse";
    properties.setStorageParameters (options);

    juce::addDefaultFormatsToManager (formatManager);

    if (auto xml = properties.getUserSettings()->getXmlValue ("knownPlugins"))
        knownPlugins.recreateFromXml (*xml);

    auto audioState = properties.getUserSettings()->getXmlValue ("audioDevice");
    const auto error = deviceManager.initialise (0, 2, audioState.get(), true);
    if (error.isNotEmpty())
    {
        DBG ("Audio device: " << error);
    }
    deviceManager.addAudioCallback (&engine);

    if (auto* t = session.selectedTrack())
    {
        t->profile = profiles.fallback();
        t->settings = defaultSettings();
    }

    knownPlugins.addChangeListener (this);
    startTimer (1000);
}

AppContext::~AppContext()
{
    stopTimer();
    knownPlugins.removeChangeListener (this);
    engine.stop();
    deviceManager.removeAudioCallback (&engine);

    if (auto xml = deviceManager.createStateXml())
        properties.getUserSettings()->setValue ("audioDevice", xml.get());
    if (auto xml = knownPlugins.createXml())
        properties.getUserSettings()->setValue ("knownPlugins", xml.get());
    properties.saveIfNeeded();

    pluginWindows.clear();
    for (auto& p : session.clear())
        p.reset();
}

void AppContext::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto xml = knownPlugins.createXml())
    {
        properties.getUserSettings()->setValue ("knownPlugins", xml.get());
        properties.saveIfNeeded();
    }
}

// --- MIDI -------------------------------------------------------------------

bool AppContext::isMidiFile (const juce::File& f)
{
    return f.hasFileExtension ("mid;midi;smf");
}

bool AppContext::loadMidi (Track& t, const juce::File& file, juce::String& error)
{
    auto read = vslf::io::readMidiFile (file);
    if (! read.ok)
    {
        error = read.error;
        return false;
    }

    const bool newFile = t.midiFile != file;
    if (newFile)
    {
        t.markings.clear();
        t.articulationMarkings.clear();
    }
    t.midiFile = file;
    t.input = std::move (read.part);
    t.readWarnings = std::move (read.warnings);
    t.hasMidi = true;
    t.name = file.getFileNameWithoutExtension();

    // Guess the instrument from the file or track name if the track still uses the generic profile.
    if (t.profile.id == "generic")
        if (const auto* best = vslf::guessProfile ((file.getFileNameWithoutExtension() + " " + juce::String (t.input.name)).toStdString(),
                                                   profiles.all()))
            t.profile = *best;

    if (newFile)
        decideControllers (t);

    engine.stop();
    session.process (t);
    return true;
}

// --- Controllers already in MIDI files ----------------------------------------

AppContext::FileControllers AppContext::fileControllers()
{
    const auto v = properties.getUserSettings()->getValue ("fileControllers", "ask");
    return v == "always" ? FileControllers::Always : v == "never" ? FileControllers::Never : FileControllers::Ask;
}

void AppContext::setFileControllers (FileControllers f)
{
    properties.getUserSettings()->setValue ("fileControllers", f == FileControllers::Always  ? "always"
                                                             : f == FileControllers::Never ? "never" : "ask");
    properties.saveIfNeeded();
    if (f == FileControllers::Ask)
        return;
    for (const auto& t : session.tracks())
        if (t->hasMidi && t->settings.keepControllers != (f == FileControllers::Never))
        {
            t->settings.keepControllers = f == FileControllers::Never;
            session.process (*t);
        }
}

void AppContext::decideControllers (Track& t)
{
    switch (fileControllers())
    {
        case FileControllers::Always: t.settings.keepControllers = false; return;
        case FileControllers::Never:  t.settings.keepControllers = true;  return;
        case FileControllers::Ask:    break;
    }
    // Removed until the answer comes; nothing to ask about without controllers.
    t.settings.keepControllers = false;
    if (! t.input.controllers.empty())
    {
        controllerQuestions.push_back (&t);
        askNextControllerQuestion();
    }
}

namespace
{
juce::String controllerSummary (const std::vector<vslf::ControllerEvent>& controllers)
{
    std::map<int, int> count;
    for (const auto& c : controllers)
        ++count[c.controller];
    juce::StringArray parts;
    for (const auto& [cc, n] : count)
    {
        juce::String name = "CC" + juce::String (cc);
        const auto known = juce::String (juce::MidiMessage::getControllerName (cc));
        if (known.isNotEmpty())
            name << " (" << known << ")";
        parts.add (name);
    }
    return parts.joinIntoString (", ");
}
} // namespace

void AppContext::askNextControllerQuestion()
{
    if (askingControllers)
        return;
    while (! controllerQuestions.empty() && session.indexOf (controllerQuestions.front()) < 0)
        controllerQuestions.pop_front(); // the track was removed meanwhile
    if (controllerQuestions.empty())
        return;

    auto* t = controllerQuestions.front();
    controllerQuestions.pop_front();
    askingControllers = true;

    const auto file = t->midiFile;
    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle ("Keep the controllers in " + file.getFileName() + "?")
                       .withMessage ("This MIDI file has controller data: " + controllerSummary (t->input.controllers) + ".\n\n"
                                     "Keep: they stay in the finessed part, except where VSL Finesse writes its own "
                                     "dynamics (CC1) and expression (CC11).\n"
                                     "Remove: only VSL Finesse's own controllers are written.\n\n"
                                     "You can choose to always or never remove them in Settings.")
                       .withButton ("Keep")
                       .withButton ("Remove");
    juce::AlertWindow::showAsync (options, [this, t, file] (int result) {
        askingControllers = false;
        // Still the same track and file? (result 1 = Keep)
        if (session.indexOf (t) >= 0 && t->midiFile == file && result == 1 && ! t->settings.keepControllers)
        {
            t->settings.keepControllers = true;
            session.process (*t);
        }
        askNextControllerQuestion();
    });
}

bool AppContext::exportFinessed (Track& t, const juce::File& file, juce::String& error)
{
    if (! t.hasMidi)
    {
        error = "There's no MIDI to export.";
        return false;
    }
    std::string e;
    if (! vslf::io::writeMidiFile (t.result.part, file, e))
    {
        error = e;
        return false;
    }
    return true;
}

// --- Profiles and settings ----------------------------------------------------

void AppContext::setProfile (Track& t, const std::string& id)
{
    if (const auto* p = profiles.find (id))
    {
        t.profile = *p;
        session.process (t);
        session.sendChangeMessage();
    }
}

void AppContext::updateProfile (Track& t, const vslf::InstrumentProfile& p)
{
    t.profile = p;
    juce::String error;
    if (! profiles.save (p, error))
    {
        DBG ("Couldn't save profile: " << error);
    }
    session.process (t);
}

void AppContext::applyMarking (Track& t, vslf::Marking m)
{
    if (! t.hasMidi)
        return;
    if (session.hasSelection() && session.selectedTrack() == &t)
    {
        m.start = session.selectionStart;
        m.end = session.selectionEnd;
    }
    else
    {
        m.start = 0;
        m.end = partEnd (t);
    }
    vslf::addMarking (t.markings, m);
    session.process (t);
}

void AppContext::clearMarkings (Track& t)
{
    if (session.hasSelection() && session.selectedTrack() == &t)
        vslf::clearMarkings (t.markings, session.selectionStart, session.selectionEnd);
    else
        t.markings.clear();
    session.process (t);
}

void AppContext::applyArticulation (Track& t, vslf::ArticulationMarking m)
{
    if (! t.hasMidi)
        return;
    if (session.hasSelection() && session.selectedTrack() == &t)
    {
        m.start = session.selectionStart;
        m.end = session.selectionEnd;
    }
    else
    {
        m.start = 0;
        m.end = partEnd (t);
    }
    vslf::addArticulationMarking (t.articulationMarkings, m);
    session.process (t);
}

void AppContext::clearArticulations (Track& t)
{
    if (session.hasSelection() && session.selectedTrack() == &t)
        vslf::clearArticulationMarkings (t.articulationMarkings, session.selectionStart, session.selectionEnd);
    else
        t.articulationMarkings.clear();
    session.process (t);
}

vslf::Tick AppContext::partEnd (const Track& t)
{
    vslf::Tick end = 0;
    for (const auto& n : t.input.notes)
        end = std::max (end, n.end);
    return end + 1;
}

void AppContext::setSettings (Track& t, const vslf::Settings& s)
{
    t.settings = s;
    session.process (t);
}

void AppContext::saveDefaultSettings (const vslf::Settings& s)
{
    properties.getUserSettings()->setValue ("defaultSettings", juce::JSON::toString (vslf::io::settingsToJson (s), true));
    properties.saveIfNeeded();
}

vslf::Settings AppContext::defaultSettings()
{
    vslf::Settings s;
    const auto text = properties.getUserSettings()->getValue ("defaultSettings");
    if (text.isNotEmpty())
        vslf::io::settingsFromJson (juce::JSON::parse (text), s);
    if (auto t = vslf::presetTypeFromString (properties.getUserSettings()->getValue ("presetType").toStdString()))
        s.presetType = *t;
    return s;
}

void AppContext::rememberPresetType (vslf::PresetType t)
{
    properties.getUserSettings()->setValue ("presetType", juce::String (vslf::toString (t)));
    properties.saveIfNeeded();
}

// --- Plugins ------------------------------------------------------------------

bool AppContext::isViennaInstrument (const juce::PluginDescription& d)
{
    const auto text = (d.name + " " + d.manufacturerName).toLowerCase();
    return text.contains ("synchron") || text.contains ("vienna") || text.contains ("vsl");
}

bool AppContext::isSynchronPlayer (const juce::PluginDescription& d)
{
    return d.name.containsIgnoreCase ("synchron player");
}

juce::Array<juce::PluginDescription> AppContext::instrumentPlugins() const
{
    juce::Array<juce::PluginDescription> result;

   #if JUCE_MAC
    // Ask macOS for the installed Audio Unit instruments. Nothing is loaded:
    // loading every plug-in to inspect it (a full scan) lets any one badly
    // behaved plug-in crash the app. Only the instrument you pick gets loaded.
    for (auto* format : formatManager.getFormats())
    {
        if (format->getName() != "AudioUnit")
            continue;

        for (const auto& id : format->searchPathsForPlugins ({}, true, true))
        {
            if (! id.contains ("/aumu,")) // instruments ("music devices") only
                continue;

            juce::PluginDescription d;
            d.pluginFormatName = format->getName();
            d.fileOrIdentifier = id;
            d.name = d.descriptiveName = format->getNameOfPluginFromIdentifier (id);
            d.manufacturerName = id.fromLastOccurrenceOf (",", false, false);
            d.category = "Synth";
            d.isInstrument = true;
            result.add (d);
        }
    }
   #else
    for (const auto& d : knownPlugins.getTypes())
        if (d.isInstrument)
            result.add (d);
   #endif

    std::sort (result.begin(), result.end(), [] (const juce::PluginDescription& a, const juce::PluginDescription& b) {
        const bool va = isViennaInstrument (a), vb = isViennaInstrument (b);
        if (va != vb)
            return va;
        return a.name.compareNatural (b.name) < 0;
    });
    return result;
}

void AppContext::showPluginScanner (juce::Component* parent)
{
    const auto deadMansPedal = ProfileLibrary::appDataFolder().getChildFile ("PluginScanCrashes.txt");
    auto* list = new juce::PluginListComponent (formatManager, knownPlugins, deadMansPedal,
                                                properties.getUserSettings(), true);
    list->setSize (720, 520);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (list);
    o.dialogTitle = "Instrument plug-ins";
    o.componentToCentreAround = parent;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

void AppContext::loadPlugin (Track& t, const juce::PluginDescription& desc,
                             std::function<void (juce::String)> done, juce::MemoryBlock state)
{
    const auto trackIndex = session.indexOf (&t);
    formatManager.createPluginInstanceAsync (
        desc, engine.getSampleRate(), engine.getBlockSize(),
        [this, trackIndex, desc, state, callback = std::move (done)] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                                                  const juce::String& message) mutable
        {
            auto* track = session.track (trackIndex);
            if (instance == nullptr || track == nullptr)
            {
                if (callback)
                    callback (message.isNotEmpty() ? message : juce::String ("The plug-in couldn't be loaded."));
                return;
            }

            if (state.getSize() > 0)
                instance->setStateInformation (state.getData(), (int) state.getSize());
            else if (isSynchronPlayer (desc))
            {
                // Synchron Player starts in its Flow view. The app is tuned for
                // the Precision presets, so open it there instead.
                juce::MemoryBlock fresh;
                instance->getStateInformation (fresh);
                if (const auto precision = vslf::io::withPrecisionView (fresh))
                    instance->setStateInformation (precision->getData(), (int) precision->getSize());
            }

            // The preset it starts with (none, or the one a session restored)
            // is taken as already matched: only later changes are followed.
            juce::String presetPath;
            std::optional<vslf::io::SynchronArticulations> tree;
            if (isSynchronPlayer (desc))
            {
                juce::MemoryBlock current;
                instance->getStateInformation (current);
                if (const auto preset = vslf::io::synchronPreset (current))
                    presetPath = preset->path;
                tree = vslf::io::synchronArticulations (current);
            }

            engine.prepare (*instance);
            closePluginWindow (track);
            track->pluginDescription = instance->getPluginDescription();
            if (track->pluginDescription.fileOrIdentifier.isEmpty())
                track->pluginDescription = desc;
            destroyPlugin (session.swapPlugin (*track, std::move (instance)));
            track->hasPlugin = true;
            // A session restores its own start articulation for the same
            // preset: the state may hold whatever was played last.
            if (presetPath != track->synchronPreset || ! tree)
                track->articulationStartKeys = tree ? tree->currentKeys : std::vector<int> {};
            track->synchronPreset = presetPath;
            track->synchronArticulations = tree ? tree->choices : std::vector<vslf::io::SynchronArticulation> {};
            session.process (*track);
            if (! isSynchronPlayer (desc))
                warnOtherInstrument (desc.name);

            if (callback)
                callback ({});
        });
}

void AppContext::loadDefaultInstrument (Track& t)
{
    if (t.hasPlugin || t.audio.plugin != nullptr)
        return;

    for (const auto& d : instrumentPlugins())
    {
        if (isSynchronPlayer (d))
        {
            loadPlugin (t, d, [] (juce::String error) {
                if (error.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                            "Couldn't load Synchron Player", error);
            });
            return;
        }
    }

    warnSynchronMissing();
}

bool AppContext::isSynchronInstalled() const
{
    for (const auto& d : instrumentPlugins())
        if (isSynchronPlayer (d))
            return true;
    return false;
}

void AppContext::warnSynchronMissing()
{
    if (std::exchange (warnedSynchronMissing, true))
        return;
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::WarningIcon, "Synchron Player isn't installed",
        "VSL Finesse was designed to use VSL Synchron Player. Please install it: Vienna Assistant installs it with your "
        "Vienna libraries.\n\nYou can pick another instrument from the track's instrument button, but other virtual "
        "instruments may not behave as intended.");
}

void AppContext::warnOtherInstrument (const juce::String& name)
{
    if (std::exchange (warnedOtherInstrument, true))
        return;
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::WarningIcon, "Not Synchron Player",
        "VSL Finesse was designed to use VSL Synchron Player. Other virtual instruments, such as " + name
            + ", may not behave as intended.");
}

namespace
{
bool sameArticulations (const std::vector<vslf::io::SynchronArticulation>& a, const std::vector<vslf::io::SynchronArticulation>& b)
{
    return std::equal (a.begin(), a.end(), b.begin(), b.end(), [] (const auto& x, const auto& y) {
        return x.path == y.path && x.keys == y.keys;
    });
}
} // namespace

void AppContext::followSynchronPreset (Track& t)
{
    auto* plugin = t.audio.plugin.get();
    if (plugin == nullptr || ! isSynchronPlayer (t.pluginDescription))
        return;

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    const auto preset = vslf::io::synchronPreset (state);
    if (! preset)
        return;

    // The articulation tree can change without a new preset (keyswitches
    // edited in Synchron). Its selection changes whenever a keyswitch plays,
    // so that's only taken when a preset is loaded.
    const auto tree = vslf::io::synchronArticulations (state);
    const bool newPreset = preset->path != t.synchronPreset;
    bool changed = false;
    if (tree && ! sameArticulations (tree->choices, t.synchronArticulations))
    {
        t.synchronArticulations = tree->choices;
        changed = true;
    }
    if (tree && newPreset)
    {
        t.articulationStartKeys = tree->currentKeys;
        changed = true;
    }
    if (! newPreset)
    {
        if (changed)
        {
            session.process (t);
            session.sendChangeMessage();
        }
        return;
    }
    t.synchronPreset = preset->path;

    if (preset->instrument.isNotEmpty())
        if (const auto* p = vslf::guessProfile (preset->instrument.toStdString(), profiles.all()); p != nullptr && p->id != t.profile.id)
        {
            t.profile = *p;
            changed = true;
        }
    if (preset->type && *preset->type != t.settings.presetType)
    {
        t.settings.presetType = *preset->type;
        rememberPresetType (*preset->type);
        changed = true;
    }
    if (changed)
    {
        session.process (t);
        session.sendChangeMessage();
    }
}

void AppContext::timerCallback()
{
    // Presets are picked in Synchron's window, so only tracks with an open
    // window are checked.
    std::vector<Track*> open;
    for (const auto& [track, window] : pluginWindows)
        open.push_back (track);
    for (auto* t : open)
        followSynchronPreset (*t);
}

void AppContext::removePlugin (Track& t)
{
    closePluginWindow (&t);
    destroyPlugin (session.swapPlugin (t, nullptr));
    t.hasPlugin = false;
    t.synchronArticulations.clear();
    session.process (t);
}

void AppContext::destroyPlugin (std::unique_ptr<juce::AudioPluginInstance> plugin)
{
    if (plugin != nullptr)
    {
        plugin->releaseResources();
        plugin.reset();
    }
}

void AppContext::showPluginWindow (Track& t)
{
    if (auto it = pluginWindows.find (&t); it != pluginWindows.end())
    {
        it->second->toFront (true);
        return;
    }

    auto* plugin = t.audio.plugin.get();
    if (plugin == nullptr)
        return;

    auto* editor = plugin->createEditorAndMakeActive();
    if (editor == nullptr)
        editor = new juce::GenericAudioProcessorEditor (*plugin);

    auto* track = &t;
    pluginWindows[&t] = std::make_unique<PluginWindow> (editor, t.name + " - " + plugin->getName(),
                                                        [this, track] {
                                                            if (session.indexOf (track) >= 0)
                                                                followSynchronPreset (*track);
                                                            closePluginWindow (track);
                                                        });
}

namespace
{
// Readable text runs in a binary blob: ASCII and UTF-16LE, at least 4 characters.
juce::StringArray readableStrings (const juce::MemoryBlock& block, int limit)
{
    juce::StringArray out;
    const auto* d = static_cast<const uint8_t*> (block.getData());
    const size_t n = block.getSize();
    auto printable = [] (uint8_t c) { return c >= 32 && c < 127; };

    std::string run;
    for (size_t i = 0; i < n && out.size() < limit; ++i)
    {
        if (printable (d[i]))
            run += (char) d[i];
        else
        {
            if (run.size() >= 4)
                out.add (juce::String (run));
            run.clear();
        }
    }
    if (run.size() >= 4)
        out.add (juce::String (run));

    for (size_t offset = 0; offset < 2; ++offset)
    {
        run.clear();
        for (size_t i = offset; i + 1 < n && out.size() < limit; i += 2)
        {
            if (printable (d[i]) && d[i + 1] == 0)
                run += (char) d[i];
            else
            {
                if (run.size() >= 4)
                    out.add ("utf16: " + juce::String (run));
                run.clear();
            }
        }
    }
    return out;
}
} // namespace

juce::File AppContext::saveInstrumentDetails (Track& t, juce::String& error)
{
    auto* plugin = t.audio.plugin.get();
    if (plugin == nullptr)
    {
        error = "This track has no instrument loaded.";
        return {};
    }

    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "VSL Finesse " + juce::JUCEApplicationBase::getInstance()->getApplicationVersion());
    o->setProperty ("plugin", plugin->getPluginDescription().createXml()->toString (juce::XmlElement::TextFormat().singleLine()));
    o->setProperty ("track", t.name);

    juce::Array<juce::var> programs;
    for (int i = 0; i < plugin->getNumPrograms(); ++i)
        programs.add (plugin->getProgramName (i));
    o->setProperty ("programs", programs);
    o->setProperty ("currentProgram", plugin->getCurrentProgram());

    juce::Array<juce::var> params;
    for (auto* p : plugin->getParameters())
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("index", p->getParameterIndex());
        po->setProperty ("name", p->getName (100));
        po->setProperty ("label", p->getLabel());
        po->setProperty ("value", p->getValue());
        po->setProperty ("text", p->getCurrentValueAsText());
        params.add (juce::var (po));
    }
    o->setProperty ("parameters", params);

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    o->setProperty ("stateSize", (int) state.getSize());
    o->setProperty ("strings", readableStrings (state, 20000));
    o->setProperty ("state", state.toBase64Encoding());

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
    auto file = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                    .getChildFile ("VSL Finesse details " + juce::File::createLegalFileName (plugin->getName()) + " " + stamp + ".json");
    if (! file.replaceWithText (juce::JSON::toString (juce::var (o))))
    {
        error = "Couldn't write " + file.getFullPathName();
        return {};
    }
    return file;
}

void AppContext::closePluginWindow (Track* t)
{
    if (auto it = pluginWindows.find (t); it != pluginWindows.end())
    {
        auto window = std::move (it->second);
        pluginWindows.erase (it);
        window.reset();
    }
}

void AppContext::removeTrack (int index)
{
    if (auto* t = session.track (index))
        closePluginWindow (t);
    destroyPlugin (session.removeTrack (index));
    if (session.numTracks() == 0)
    {
        auto& t = session.addTrack();
        t.profile = profiles.fallback();
        t.settings = defaultSettings();
    }
}

// --- Sessions -------------------------------------------------------------------

bool AppContext::saveSession (const juce::File& file, juce::String& error)
{
    juce::XmlElement root ("VSLFinesseSession");
    root.setAttribute ("version", 1);

    for (const auto& t : session.tracks())
    {
        auto* x = root.createNewChildElement ("Track");
        x->setAttribute ("name", t->name);
        x->setAttribute ("midi", t->midiFile.getFullPathName());
        x->setAttribute ("profile", juce::String (t->profile.id));
        x->setAttribute ("mute", t->audio.mute);
        x->setAttribute ("solo", t->audio.solo);
        x->setAttribute ("gain", (double) t->audio.gain);
        x->createNewChildElement ("Settings")->addTextElement (juce::JSON::toString (vslf::io::settingsToJson (t->settings), true));
        x->createNewChildElement ("Markings")->addTextElement (juce::JSON::toString (vslf::io::markingsToJson (t->markings), true));
        x->createNewChildElement ("Articulations")->addTextElement (
            juce::JSON::toString (vslf::io::articulationMarkingsToJson (t->articulationMarkings), true));
        x->setAttribute ("synchronPreset", t->synchronPreset);
        juce::StringArray startKeys;
        for (int k : t->articulationStartKeys)
            startKeys.add (juce::String (k));
        x->setAttribute ("articulationStartKeys", startKeys.joinIntoString (" "));

        if (t->hasPlugin && t->audio.plugin != nullptr)
        {
            auto* px = x->createNewChildElement ("Plugin");
            px->addChildElement (t->pluginDescription.createXml().release());
            juce::MemoryBlock state;
            t->audio.plugin->getStateInformation (state);
            px->createNewChildElement ("State")->addTextElement (state.toBase64Encoding());
        }
    }

    if (! root.writeTo (file))
    {
        error = "Couldn't write " + file.getFullPathName();
        return false;
    }
    currentSessionFile = file;
    return true;
}

void AppContext::openSession (const juce::File& file, std::function<void (juce::String)> done)
{
    auto root = juce::XmlDocument::parse (file);
    // "VSLPerformerSession": sessions saved before the app was renamed.
    if (root == nullptr || ! (root->hasTagName ("VSLFinesseSession") || root->hasTagName ("VSLPerformerSession")))
    {
        done ("Not a VSL Finesse session: " + file.getFileName());
        return;
    }

    engine.stop();
    pluginWindows.clear();
    for (auto& p : session.clear())
        destroyPlugin (std::move (p));

    auto errors = std::make_shared<juce::StringArray>();
    auto pending = std::make_shared<int> (0);
    auto finished = std::make_shared<bool> (false);
    auto finish = [done, errors, pending, finished] {
        if (*pending == 0 && *finished)
            done (errors->joinIntoString ("\n"));
    };

    for (auto* x : root->getChildWithTagNameIterator ("Track"))
    {
        auto& t = session.addTrack();
        t.name = x->getStringAttribute ("name", t.name);
        t.audio.mute = x->getBoolAttribute ("mute");
        t.audio.solo = x->getBoolAttribute ("solo");
        t.audio.gain = (float) x->getDoubleAttribute ("gain", 1.0);

        const auto* p = profiles.find (x->getStringAttribute ("profile").toStdString());
        t.profile = p != nullptr ? *p : profiles.fallback();

        t.settings = defaultSettings();
        t.settings.keepControllers = false; // sessions saved before the setting existed removed them
        if (auto* sx = x->getChildByName ("Settings"))
            vslf::io::settingsFromJson (juce::JSON::parse (sx->getAllSubText()), t.settings);
        if (auto* mx = x->getChildByName ("Markings"))
            t.markings = vslf::io::markingsFromJson (juce::JSON::parse (mx->getAllSubText()));
        if (auto* ax = x->getChildByName ("Articulations"))
            t.articulationMarkings = vslf::io::articulationMarkingsFromJson (juce::JSON::parse (ax->getAllSubText()));
        t.synchronPreset = x->getStringAttribute ("synchronPreset");
        for (const auto& k : juce::StringArray::fromTokens (x->getStringAttribute ("articulationStartKeys"), " ", ""))
            if (k.isNotEmpty())
                t.articulationStartKeys.push_back (k.getIntValue());

        const juce::File midi (x->getStringAttribute ("midi"));
        if (midi.existsAsFile())
        {
            const auto profile = t.profile;
            juce::String error;
            t.midiFile = midi; // the same file: its markings stay
            if (! loadMidi (t, midi, error))
                errors->add (error);
            if (t.profile.id != profile.id) // keep the saved instrument, not the guessed one
            {
                t.profile = profile;
                session.process (t);
            }
        }
        else if (x->getStringAttribute ("midi").isNotEmpty())
            errors->add ("Missing MIDI file: " + midi.getFullPathName());

        if (auto* px = x->getChildByName ("Plugin"))
        {
            juce::PluginDescription desc;
            auto* dx = px->getChildByName ("PLUGIN");
            if (dx != nullptr && desc.loadFromXml (*dx))
            {
                juce::MemoryBlock state;
                if (auto* stx = px->getChildByName ("State"))
                    state.fromBase64Encoding (stx->getAllSubText());

                ++*pending;
                loadPlugin (t, desc, [errors, pending, finish] (juce::String error) {
                    if (error.isNotEmpty())
                        errors->add (error);
                    --*pending;
                    finish();
                }, state);
            }
        }
    }

    if (session.numTracks() == 0)
    {
        auto& t = session.addTrack();
        t.profile = profiles.fallback();
        t.settings = defaultSettings();
    }

    session.select (0);
    currentSessionFile = file;
    *finished = true;
    finish();
}
