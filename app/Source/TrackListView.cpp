#include "TrackListView.h"
#include "Theme.h"

namespace
{
constexpr int rowHeight = 44;
}

class TrackListView::Row : public juce::Component
{
public:
    Row (AppContext& a, TrackListView& o, int i) : app (a), owner (o), index (i)
    {
        addAndMakeVisible (nameLabel);
        nameLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        nameLabel.setColour (juce::Label::textColourId, theme::text);
        nameLabel.setEditable (false, true);
        nameLabel.onTextChange = [this] {
            if (auto* t = track())
            {
                t->name = nameLabel.getText();
                app.session.sendChangeMessage();
            }
        };
        nameLabel.addMouseListener (this, false);

        addAndMakeVisible (fileLabel);
        fileLabel.setFont (juce::FontOptions (11.0f));
        fileLabel.setColour (juce::Label::textColourId, theme::textDim);
        fileLabel.addMouseListener (this, false);

        addAndMakeVisible (midiButton);
        midiButton.setTooltip ("Open a MIDI file for this track (or drop one here)");
        midiButton.onClick = [this] {
            if (auto* t = track(); t != nullptr && owner.onOpenMidi)
                owner.onOpenMidi (*t);
        };

        addAndMakeVisible (pluginButton);
        pluginButton.setTooltip ("Instrument plug-in that plays this track (e.g. Synchron Player)");
        pluginButton.onClick = [this] { showPluginMenu(); };

        addAndMakeVisible (editButton);
        editButton.setTooltip ("Show the instrument's window");
        editButton.onClick = [this] {
            if (auto* t = track())
                app.showPluginWindow (*t);
        };

        for (auto* b : { &muteButton, &soloButton })
        {
            addAndMakeVisible (b);
            b->setClickingTogglesState (true);
        }
        muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange.darker());
        soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::gold.darker());
        muteButton.onClick = [this] { setFlag (true, muteButton.getToggleState()); };
        soloButton.onClick = [this] { setFlag (false, soloButton.getToggleState()); };

        addAndMakeVisible (removeButton);
        removeButton.setTooltip ("Remove this track");
        removeButton.onClick = [this] {
            juce::MessageManager::callAsync ([&ctx = app, trackNumber = index] { ctx.removeTrack (trackNumber); });
        };

        refresh();
    }

    Track* track() const { return app.session.track (index); }

    void refresh()
    {
        auto* t = track();
        if (t == nullptr)
            return;
        nameLabel.setText (t->name, juce::dontSendNotification);
        fileLabel.setText (t->hasMidi ? t->midiFile.getFileName() + "  -  " + juce::String (t->profile.name) : "No MIDI yet",
                           juce::dontSendNotification);
        pluginButton.setButtonText (loading ? "Loading..." : t->hasPlugin ? t->pluginDescription.name : "Choose instrument...");
        editButton.setEnabled (t->audio.plugin != nullptr);
        muteButton.setToggleState (t->audio.mute, juce::dontSendNotification);
        soloButton.setToggleState (t->audio.solo, juce::dontSendNotification);
        removeButton.setVisible (app.session.numTracks() > 1);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const bool selected = app.session.selectedIndex() == index;
        g.setColour (selected ? theme::panelRaised : theme::panel);
        g.fillRect (getLocalBounds());
        if (selected)
        {
            g.setColour (theme::accent);
            g.fillRect (0, 0, 3, getHeight());
        }
        g.setColour (theme::outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        removeButton.setBounds (r.removeFromRight (24).withSizeKeepingCentre (24, 24));
        r.removeFromRight (6);
        soloButton.setBounds (r.removeFromRight (28).withSizeKeepingCentre (28, 24));
        r.removeFromRight (2);
        muteButton.setBounds (r.removeFromRight (28).withSizeKeepingCentre (28, 24));
        r.removeFromRight (10);
        editButton.setBounds (r.removeFromRight (48).withSizeKeepingCentre (48, 24));
        r.removeFromRight (4);
        pluginButton.setBounds (r.removeFromRight (juce::jmin (220, r.getWidth() / 3)).withSizeKeepingCentre (juce::jmin (220, r.getWidth() / 3), 24));
        r.removeFromRight (8);
        midiButton.setBounds (r.removeFromRight (64).withSizeKeepingCentre (64, 24));
        r.removeFromRight (8);
        nameLabel.setBounds (r.removeFromTop (r.getHeight() / 2 + 2));
        fileLabel.setBounds (r);
    }

    void mouseDown (const juce::MouseEvent&) override { app.session.select (index); }

    bool loading = false;

private:
    void setFlag (bool mute, bool on)
    {
        if (auto* t = track())
        {
            const juce::ScopedLock sl (app.session.audioLock);
            (mute ? t->audio.mute : t->audio.solo) = on;
        }
    }

    void showPluginMenu()
    {
        auto* t = track();
        if (t == nullptr)
            return;

        app.session.select (index);
        const auto types = app.instrumentPlugins();

        juce::PopupMenu menu;
        juce::PopupMenu others;
        bool anyVienna = false;
        for (int i = 0; i < types.size(); ++i)
        {
            const auto& d = types.getReference (i);
            const bool ticked = t->hasPlugin && d.fileOrIdentifier == t->pluginDescription.fileOrIdentifier;
            if (AppContext::isViennaInstrument (d))
            {
                if (! anyVienna)
                    menu.addSectionHeader ("Vienna Symphonic Library");
                anyVienna = true;
                menu.addItem (i + 1, d.name, true, ticked);
            }
            else
                others.addItem (i + 1, d.name, true, ticked);
        }

        if (types.isEmpty())
           #if JUCE_MAC
            menu.addItem ("No Audio Unit instruments found", false, false, nullptr);
           #else
            menu.addItem ("No instruments found yet - scan first", false, false, nullptr);
           #endif
        else if (others.getNumItems() > 0)
            menu.addSubMenu (anyVienna ? "Other instruments" : "Instruments", others);

        menu.addSeparator();
       #if ! JUCE_MAC
        menu.addItem (-1, "Scan for plug-ins...");
       #endif
        menu.addItem (-2, "Remove instrument", t->audio.plugin != nullptr);
        menu.addSeparator();
        menu.addItem (-3, "Save instrument details to Desktop", t->audio.plugin != nullptr);

        juce::Component::SafePointer<Row> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&pluginButton),
                            [safe, types] (int result) {
                                if (safe == nullptr || result == 0)
                                    return;
                                safe->menuChosen (result, types);
                            });
    }

    void menuChosen (int result, const juce::Array<juce::PluginDescription>& types)
    {
        auto* t = track();
        if (t == nullptr)
            return;

        if (result == -1)
        {
            app.showPluginScanner (getTopLevelComponent());
            return;
        }
        if (result == -3)
        {
            juce::String error;
            const auto file = app.saveInstrumentDetails (*t, error);
            if (file.existsAsFile())
                file.revealToUser();
            else
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save the details", error);
            return;
        }
        if (result == -2)
        {
            app.removePlugin (*t);
            refresh();
            return;
        }

        const int chosen = result - 1;
        if (! juce::isPositiveAndBelow (chosen, types.size()))
            return;

        loading = true;
        refresh();
        juce::Component::SafePointer<Row> safe (this);
        app.loadPlugin (*t, types[chosen], [safe, &a = app, i = index] (juce::String error) {
            if (safe != nullptr)
            {
                safe->loading = false;
                safe->refresh();
            }
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the instrument", error);
            else if (auto* tr = a.session.track (i))
                a.showPluginWindow (*tr);
        });
    }

    AppContext& app;
    TrackListView& owner;
    int index;

    juce::Label nameLabel, fileLabel;
    juce::TextButton midiButton { "MIDI..." }, pluginButton, editButton { "Edit" };
    juce::TextButton muteButton { "M" }, soloButton { "S" }, removeButton { "x" };
};

// ---------------------------------------------------------------------------
TrackListView::TrackListView (AppContext& a) : app (a)
{
    addAndMakeVisible (addButton);
    addButton.setTooltip ("Add another part (each track has its own instrument)");
    addButton.onClick = [this] {
        auto& t = app.session.addTrack();
        t.profile = app.profiles.fallback();
        t.settings = app.defaultSettings();
    };
    app.session.addChangeListener (this);
    rebuild();
}

TrackListView::~TrackListView()
{
    app.session.removeChangeListener (this);
}

int TrackListView::preferredHeight() const
{
    return rows.size() * rowHeight + 34;
}

void TrackListView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (rows.size() != app.session.numTracks())
        rebuild();
    else
        for (auto* r : rows)
            r->refresh();
}

void TrackListView::rebuild()
{
    rows.clear();
    for (int i = 0; i < app.session.numTracks(); ++i)
        addAndMakeVisible (rows.add (new Row (app, *this, i)));
    resized();
    if (onLayoutChanged)
        onLayoutChanged();
}

void TrackListView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
}

void TrackListView::resized()
{
    auto r = getLocalBounds();
    for (auto* row : rows)
        row->setBounds (r.removeFromTop (rowHeight));
    addButton.setBounds (r.removeFromTop (34).reduced (8, 5).removeFromLeft (110));
}

int TrackListView::trackAt (juce::Point<int> p) const
{
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i]->getBounds().contains (p))
            return i;
    return -1;
}
