#include "AppContext.h"

#include <JsonIO.h>
#include <MidiIO.h>

#include <algorithm>

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
    juce::PropertiesFile::Options options;
    options.applicationName = "VSL Performer";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "VSL Performer";
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
}

AppContext::~AppContext()
{
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
    auto read = vslp::io::readMidiFile (file);
    if (! read.ok)
    {
        error = read.error;
        return false;
    }

    if (t.midiFile != file)
        t.markings.clear();
    t.midiFile = file;
    t.input = std::move (read.part);
    t.readWarnings = std::move (read.warnings);
    t.hasMidi = true;
    t.name = file.getFileNameWithoutExtension();

    // Guess the instrument from the file or track name if the track still uses the generic profile.
    if (t.profile.id == "generic")
    {
        const auto haystack = (file.getFileNameWithoutExtension() + " " + juce::String (t.input.name)).toLowerCase();
        const vslp::InstrumentProfile* best = nullptr;
        for (const auto& p : profiles.all())
        {
            if (p.id == "generic")
                continue;
            const auto name = juce::String (p.name).toLowerCase();
            const auto singular = name.trimCharactersAtEnd ("s");
            if (haystack.contains (name) || haystack.contains (singular) || haystack.contains (juce::String (p.id)))
                if (best == nullptr || p.name.size() > best->name.size()) // prefer "bass clarinet" over "clarinet"
                    best = &p;
        }
        if (best != nullptr)
            t.profile = *best;
    }

    engine.stop();
    session.process (t);
    return true;
}

bool AppContext::exportFinessed (Track& t, const juce::File& file, juce::String& error)
{
    if (! t.hasMidi)
    {
        error = "There's no MIDI to export.";
        return false;
    }
    std::string e;
    if (! vslp::io::writeMidiFile (t.result.part, file, e))
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

void AppContext::updateProfile (Track& t, const vslp::InstrumentProfile& p)
{
    t.profile = p;
    juce::String error;
    if (! profiles.save (p, error))
    {
        DBG ("Couldn't save profile: " << error);
    }
    session.process (t);
}

void AppContext::applyMarking (Track& t, vslp::Marking m)
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
    vslp::addMarking (t.markings, m);
    session.process (t);
}

void AppContext::clearMarkings (Track& t)
{
    if (session.hasSelection() && session.selectedTrack() == &t)
        vslp::clearMarkings (t.markings, session.selectionStart, session.selectionEnd);
    else
        t.markings.clear();
    session.process (t);
}

vslp::Tick AppContext::partEnd (const Track& t)
{
    vslp::Tick end = 0;
    for (const auto& n : t.input.notes)
        end = std::max (end, n.end);
    return end + 1;
}

void AppContext::setSettings (Track& t, const vslp::Settings& s)
{
    t.settings = s;
    session.process (t);
}

void AppContext::saveDefaultSettings (const vslp::Settings& s)
{
    properties.getUserSettings()->setValue ("defaultSettings", juce::JSON::toString (vslp::io::settingsToJson (s), true));
    properties.saveIfNeeded();
}

vslp::Settings AppContext::defaultSettings()
{
    vslp::Settings s;
    const auto text = properties.getUserSettings()->getValue ("defaultSettings");
    if (text.isNotEmpty())
        vslp::io::settingsFromJson (juce::JSON::parse (text), s);
    if (auto t = vslp::presetTypeFromString (properties.getUserSettings()->getValue ("presetType").toStdString()))
        s.presetType = *t;
    return s;
}

void AppContext::rememberPresetType (vslp::PresetType t)
{
    properties.getUserSettings()->setValue ("presetType", juce::String (vslp::toString (t)));
    properties.saveIfNeeded();
}

// --- Plugins ------------------------------------------------------------------

bool AppContext::isViennaInstrument (const juce::PluginDescription& d)
{
    const auto text = (d.name + " " + d.manufacturerName).toLowerCase();
    return text.contains ("synchron") || text.contains ("vienna") || text.contains ("vsl");
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

            engine.prepare (*instance);
            closePluginWindow (track);
            track->pluginDescription = instance->getPluginDescription();
            if (track->pluginDescription.fileOrIdentifier.isEmpty())
                track->pluginDescription = desc;
            destroyPlugin (session.swapPlugin (*track, std::move (instance)));
            track->hasPlugin = true;

            if (callback)
                callback ({});
        });
}

void AppContext::removePlugin (Track& t)
{
    closePluginWindow (&t);
    destroyPlugin (session.swapPlugin (t, nullptr));
    t.hasPlugin = false;
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
                                                        [this, track] { closePluginWindow (track); });
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
    o->setProperty ("app", "VSL Performer " + juce::JUCEApplicationBase::getInstance()->getApplicationVersion());
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
                    .getChildFile ("VSL Performer details " + juce::File::createLegalFileName (plugin->getName()) + " " + stamp + ".json");
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
    juce::XmlElement root ("VSLPerformerSession");
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
        x->createNewChildElement ("Settings")->addTextElement (juce::JSON::toString (vslp::io::settingsToJson (t->settings), true));
        x->createNewChildElement ("Markings")->addTextElement (juce::JSON::toString (vslp::io::markingsToJson (t->markings), true));

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
    if (root == nullptr || ! root->hasTagName ("VSLPerformerSession"))
    {
        done ("Not a VSL Performer session: " + file.getFileName());
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
        if (auto* sx = x->getChildByName ("Settings"))
            vslp::io::settingsFromJson (juce::JSON::parse (sx->getAllSubText()), t.settings);
        if (auto* mx = x->getChildByName ("Markings"))
            t.markings = vslp::io::markingsFromJson (juce::JSON::parse (mx->getAllSubText()));

        const juce::File midi (x->getStringAttribute ("midi"));
        if (midi.existsAsFile())
        {
            const auto profile = t.profile;
            juce::String error;
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
