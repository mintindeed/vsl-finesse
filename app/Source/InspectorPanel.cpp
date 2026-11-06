#include "InspectorPanel.h"
#include "Theme.h"

#include <algorithm>
#include <iterator>

namespace
{

juce::String keysText (const std::vector<int>& keys)
{
    if (keys.empty())
        return "Before first keyswitch";
    juce::StringArray names;
    for (int k : keys)
        names.add (vslp::noteName (k));
    return names.joinIntoString (" + ");
}

struct SliderSpec
{
    const char* label;
    double vslp::Settings::* field;
    double min, max, step;
    const char* suffix;
    const char* tooltip;
};

const SliderSpec sliderSpecs[] = {
    { "Intensity",          &vslp::Settings::intensity,             0.0, 2.0, 0.01, "",    "Overall amount of shaping on top of the base dynamic" },
    { "Dynamic offset",     &vslp::Settings::levelOffset,          -0.5, 0.5, 0.01, "",    "Raise or lower the whole part's dynamic" },
    { "Phrase arch",        &vslp::Settings::phraseArchAmount,      0.0, 2.0, 0.01, "",    "Grow towards each phrase's peak and relax after it" },
    { "Long-note swells",   &vslp::Settings::longNoteSwellAmount,   0.0, 2.0, 0.01, "",    "Gentle swell inside long notes (CC1, or CC11 in Velocity presets)" },
    { "Phrase-end taper",   &vslp::Settings::phraseEndTaperAmount,  0.0, 2.0, 0.01, "",    "Fade the last note of each phrase (CC1 and CC11)" },
    { "Metric accents",     &vslp::Settings::metricAccentAmount,    0.0, 2.0, 0.01, "",    "Slightly stronger velocity on downbeats (where velocity is the dynamic)" },
    { "Higher = louder",    &vslp::Settings::highLoudAmount,        0.0, 2.0, 0.01, "",    "Higher notes in a phrase a little louder" },
    { "Humanize velocity",  &vslp::Settings::humanizeVelocity,      0.0, 1.0, 0.01, "",    "Small random velocity variation (repeatable via the seed)" },
    { "Humanize timing",    &vslp::Settings::humanizeTimingMs,      0.0, 30.0, 0.5, " ms", "Maximum random shift of note starts" },
    { "Note overlap",       &vslp::Settings::legatoOverlapMs,       0.0, 150.0, 1.0, " ms", "Connected notes overlap by this much, which triggers legato transitions in legato patches" },
    { "Connect gaps up to", &vslp::Settings::legatoMaxGapMs,        0.0, 300.0, 1.0, " ms", "Notes with gaps up to this long count as connected" },
};

class SectionHeader : public juce::Component
{
public:
    explicit SectionHeader (juce::String t) : title (std::move (t)) {}
    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title.toUpperCase(), getLocalBounds().withTrimmedTop (6), juce::Justification::bottomLeft);
        g.setColour (theme::outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }
    juce::String title;
};

} // namespace

// ---------------------------------------------------------------------------
class ArticulationRow : public juce::Component
{
public:
    void set (const vslp::ArticulationUsage& usage, int colourIndex)
    {
        keys = usage.keys;
        text = keysText (usage.keys);
        count = juce::String (usage.notes) + (usage.notes == 1 ? " note" : " notes");
        colour = theme::articulation (colourIndex);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (colour);
        g.fillRoundedRectangle (r.removeFromLeft (12).withSizeKeepingCentre (10, 10).toFloat(), 2.0f);
        r.removeFromLeft (8);
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (theme::textDim);
        g.drawText (count, r, juce::Justification::centredRight);
        g.setColour (theme::text);
        g.drawText (text, r, juce::Justification::centredLeft);
    }

    std::vector<int> keys;

private:
    juce::String text, count;
    juce::Colour colour;
};

// ---------------------------------------------------------------------------
class InspectorPanel::Content : public juce::Component
{
public:
    Content (AppContext& a, InspectorPanel& o) : app (a), owner (o)
    {
        for (auto* h : { &instrumentHeader, &articulationHeader, &performanceHeader, &reportHeader })
            addAndMakeVisible (h);

        addAndMakeVisible (profileBox);
        profileBox.onChange = [this] {
            auto* t = track();
            const int i = profileBox.getSelectedId() - 1;
            if (t != nullptr && juce::isPositiveAndBelow (i, (int) app.profiles.all().size()))
                app.setProfile (*t, app.profiles.all()[(size_t) i].id);
        };

        for (auto* l : { &rangeLabel, &articulationHint })
        {
            addAndMakeVisible (l);
            l->setFont (juce::FontOptions (12.0f));
            l->setColour (juce::Label::textColourId, theme::textDim);
        }
        articulationHint.setText ("Keyswitches in the file (colours match the notes). "
                                  "They're kept exactly as written.", juce::dontSendNotification);

        addAndMakeVisible (presetLabel);
        presetLabel.setFont (juce::FontOptions (12.0f));
        presetLabel.setColour (juce::Label::textColourId, theme::textDim);
        presetLabel.setText ("Synchron preset folder", juce::dontSendNotification);

        const vslp::PresetType types[] = { vslp::PresetType::VelXFSustain, vslp::PresetType::VelXF, vslp::PresetType::Velocity };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = presetButtons.add (new juce::TextButton (vslp::displayName (types[i])));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (2001);
            b->setColour (juce::TextButton::buttonOnColourId, theme::accent.darker (0.3f));
            b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 2 ? juce::Button::ConnectedOnRight : 0));
            const auto type = types[i];
            b->onClick = [this, b, type] {
                if (! b->getToggleState() || pending.presetType == type)
                    return;
                pending.presetType = type;
                app.rememberPresetType (type);
                settingsChanged();
            };
            addAndMakeVisible (b);
        }
        presetButtons[0]->setTooltip ("01 VelXF sus - MOD: long notes crossfade with CC1, short notes follow velocity");
        presetButtons[1]->setTooltip ("02 VelXF - MOD: every note crossfades with CC1; velocities are left as written");
        presetButtons[2]->setTooltip ("03 Velocity: velocity picks the dynamic; no CC1 is written");
        articulationHint.setMinimumHorizontalScale (1.0f);

        for (auto* l : { &lowEditor, &highEditor })
        {
            addAndMakeVisible (l);
            l->setEditable (true);
            l->setJustificationType (juce::Justification::centred);
            l->setColour (juce::Label::backgroundColourId, theme::panelRaised);
            l->setColour (juce::Label::outlineColourId, theme::outline);
            l->setTooltip ("Lowest / highest playable note (Logic naming, C3 = middle C). Notes outside are keyswitches.");
            l->onTextChange = [this] { applyRange(); };
        }

        addAndMakeVisible (revertButton);
        revertButton.setTooltip ("Discard your changes to this instrument profile");
        revertButton.onClick = [this] {
            if (auto* t = track())
            {
                const auto id = t->profile.id;
                app.profiles.revertToBuiltin (id);
                app.setProfile (*t, id);
                refreshAll();
            }
        };

        addAndMakeVisible (folderButton);
        folderButton.setTooltip ("Your instrument profiles are JSON files in this folder");
        folderButton.onClick = [this] {
            app.profiles.folder().createDirectory();
            app.profiles.folder().revealToUser();
        };

        for (const auto& spec : sliderSpecs)
        {
            auto* label = labels.add (new juce::Label ({}, spec.label));
            label->setFont (juce::FontOptions (12.0f));
            label->setColour (juce::Label::textColourId, theme::text);
            label->setTooltip (spec.tooltip);
            addAndMakeVisible (label);

            auto* s = sliders.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight));
            s->setRange (spec.min, spec.max, spec.step);
            s->setTextValueSuffix (spec.suffix);
            s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
            s->setTooltip (spec.tooltip);
            s->setDoubleClickReturnValue (true, vslp::Settings {}.*spec.field);
            const auto field = spec.field;
            s->onValueChange = [this, s, field] { pending.*field = s->getValue(); settingsChanged(); };
            addAndMakeVisible (s);
        }

        addAndMakeVisible (inputVelocityToggle);
        inputVelocityToggle.setTooltip ("Use the file's velocities as the base dynamic (keeps dynamics written in the notation)");
        inputVelocityToggle.onClick = [this] {
            pending.useInputVelocity = inputVelocityToggle.getToggleState();
            settingsChanged();
        };

        addAndMakeVisible (variationButton);
        variationButton.setTooltip ("Re-roll the humanization");
        variationButton.onClick = [this] {
            pending.seed = pending.seed * 1664525u + 1013904223u;
            settingsChanged();
        };

        addAndMakeVisible (defaultButton);
        defaultButton.setTooltip ("Use these settings for new tracks");
        defaultButton.onClick = [this] { app.saveDefaultSettings (pending); };

        addAndMakeVisible (resetButton);
        resetButton.onClick = [this] {
            const auto seed = pending.seed;
            pending = vslp::Settings {};
            pending.seed = seed;
            showSettings();
            settingsChanged();
        };

        addAndMakeVisible (report);
        report.setMultiLine (true);
        report.setReadOnly (true);
        report.setCaretVisible (false);
        report.setColour (juce::TextEditor::backgroundColourId, theme::panel);
        report.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        report.setColour (juce::TextEditor::textColourId, theme::textDim);
        report.setFont (juce::FontOptions (12.0f));

        refreshAll();
    }

    Track* track() const { return app.session.selectedTrack(); }

    void refreshAll()
    {
        shownTrack = track();
        if (shownTrack == nullptr)
            return;
        fillProfileBox();
        pending = shownTrack->settings;
        showSettings();
        refreshResult();
    }

    void refreshResult()
    {
        auto* t = track();
        if (t == nullptr)
            return;
        if (t != shownTrack)
        {
            refreshAll();
            return;
        }

        fillProfileBox();
        lowEditor.setText (vslp::noteName (t->profile.lowestNote), juce::dontSendNotification);
        highEditor.setText (vslp::noteName (t->profile.highestNote), juce::dontSendNotification);
        rangeLabel.setText ("Range", juce::dontSendNotification);
        revertButton.setVisible (app.profiles.isCustomised (t->profile.id));

        // Articulation legend.
        const auto& usage = t->result.report.articulations;
        rows.clear();
        for (size_t i = 0; i < usage.size(); ++i)
        {
            if (usage[i].keys.empty() && usage.size() > 1 && usage[i].notes == 0)
                continue;
            auto* row = rows.add (new ArticulationRow());
            row->set (usage[i], (int) i);
            addAndMakeVisible (row);
        }

        report.setText (reportText (*t), false);
        layout();
    }

    void fillProfileBox()
    {
        auto* t = track();
        if (t == nullptr || profileBox.isPopupActive())
            return;

        profileBox.clear (juce::dontSendNotification);
        juce::String family;
        int id = 1;
        for (const auto& p : app.profiles.all())
        {
            const juce::String f = vslp::toString (p.family);
            if (f != family)
            {
                family = f;
                profileBox.addSectionHeading (family.substring (0, 1).toUpperCase() + family.substring (1));
            }
            profileBox.addItem (juce::String (p.name) + (app.profiles.isCustomised (p.id) ? "  (edited)" : ""), id);
            if (p.id == t->profile.id)
                profileBox.setSelectedId (id, juce::dontSendNotification);
            ++id;
        }
    }

    void showSettings()
    {
        for (size_t i = 0; i < std::size (sliderSpecs); ++i)
            sliders[(int) i]->setValue (pending.*(sliderSpecs[i].field), juce::dontSendNotification);
        inputVelocityToggle.setToggleState (pending.useInputVelocity, juce::dontSendNotification);
        const int typeIndex = pending.presetType == vslp::PresetType::VelXFSustain ? 0
                            : pending.presetType == vslp::PresetType::VelXF ? 1 : 2;
        presetButtons[typeIndex]->setToggleState (true, juce::dontSendNotification);
    }

    void applyPendingSettings()
    {
        if (auto* t = track(); t != nullptr && t == shownTrack)
            app.setSettings (*t, pending);
    }

    void paint (juce::Graphics& g) override { g.fillAll (theme::panel); }
    void resized() override { layout(); }

    int contentHeight = 600;

private:
    void settingsChanged() { owner.startTimer (120); }

    void applyRange()
    {
        auto* t = track();
        if (t == nullptr)
            return;
        const auto low = vslp::parseNoteName (lowEditor.getText().toStdString());
        const auto high = vslp::parseNoteName (highEditor.getText().toStdString());
        if (! low || ! high || *low > *high)
        {
            refreshResult(); // put the old values back
            return;
        }
        auto p = t->profile;
        p.lowestNote = *low;
        p.highestNote = *high;
        app.updateProfile (*t, p);
    }

    static juce::String reportText (const Track& t)
    {
        if (! t.hasMidi)
            return "No MIDI loaded.";
        const auto& r = t.result.report;
        juce::String s;
        s << r.notesOut << " notes, " << (int) t.result.phrases.size() << " phrases, "
          << r.keyswitchesIn << " keyswitches kept\n";
        s << (int) t.result.part.controllers.size() << " controller events written";
        if (r.controllersRemoved > 0)
            s << ", " << r.controllersRemoved << " old ones removed";
        s << "\n";
        if (r.legatoConnections > 0)
            s << r.legatoConnections << " legato connections\n";
        if (r.chordNotesRemoved > 0 || r.overlapsTrimmed > 0)
            s << r.chordNotesRemoved << " chord notes removed, " << r.overlapsTrimmed << " overlaps trimmed\n";
        for (const auto& w : t.readWarnings)
            s << "\n- " << w;
        for (const auto& w : r.warnings)
            s << "\n- " << w;
        return s;
    }

    void layout()
    {
        if (inLayout)
            return;
        const juce::ScopedValueSetter<bool> guard (inLayout, true);
        const int w = getWidth();
        int y = 0;
        auto place = [&] (juce::Component& c, int h, int gap = 6) {
            c.setBounds (12, y, w - 24, h);
            y += h + gap;
        };

        place (instrumentHeader, 28, 8);
        place (profileBox, 26);
        {
            juce::Rectangle<int> r (12, y, w - 24, 24);
            rangeLabel.setBounds (r.removeFromLeft (50));
            lowEditor.setBounds (r.removeFromLeft (52));
            r.removeFromLeft (6);
            highEditor.setBounds (r.removeFromLeft (52));
            r.removeFromLeft (10);
            folderButton.setBounds (r.removeFromRight (70));
            r.removeFromRight (6);
            if (revertButton.isVisible())
                revertButton.setBounds (r.removeFromRight (70));
            y += 34;
        }
        place (presetLabel, 18, 2);
        {
            juce::Rectangle<int> r (12, y, w - 24, 28);
            const int bw = r.getWidth() / 3;
            for (auto* b : presetButtons)
                b->setBounds (r.removeFromLeft (bw));
            y += 34;
        }

        place (articulationHeader, 28, 6);
        place (articulationHint, 32, 4);
        for (auto* row : rows)
            place (*row, 20, 2);

        y += 6;
        place (performanceHeader, 28, 8);
        place (inputVelocityToggle, 22, 6);
        for (int i = 0; i < sliders.size(); ++i)
        {
            labels[i]->setBounds (12, y, 120, 24);
            sliders[i]->setBounds (132, y, w - 144, 24);
            y += 28;
        }
        {
            juce::Rectangle<int> r (12, y + 4, w - 24, 26);
            const int bw = (r.getWidth() - 12) / 3;
            variationButton.setBounds (r.removeFromLeft (bw));
            r.removeFromLeft (6);
            resetButton.setBounds (r.removeFromLeft (bw));
            r.removeFromLeft (6);
            defaultButton.setBounds (r);
            y += 40;
        }

        place (reportHeader, 28, 6);
        const int reportHeight = juce::jmax (80, (int) report.getTextHeight() + 16);
        place (report, reportHeight, 12);

        if (contentHeight != y)
        {
            contentHeight = y;
            setSize (w, y);
        }
    }

    AppContext& app;
    InspectorPanel& owner;
    Track* shownTrack = nullptr;
    vslp::Settings pending;
    bool inLayout = false;

    SectionHeader instrumentHeader { "Instrument" }, articulationHeader { "Articulations" },
                  performanceHeader { "Performance" }, reportHeader { "Report" };
    juce::ComboBox profileBox;
    juce::Label rangeLabel, lowEditor, highEditor, articulationHint, presetLabel;
    juce::OwnedArray<juce::TextButton> presetButtons;
    juce::TextButton revertButton { "Revert" }, folderButton { "Folder" };
    juce::OwnedArray<ArticulationRow> rows;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::Slider> sliders;
    juce::ToggleButton inputVelocityToggle { "Base dynamic from the file's velocities" };
    juce::TextButton variationButton { "New variation" }, resetButton { "Reset" }, defaultButton { "Save as default" };
    juce::TextEditor report;
};

// ---------------------------------------------------------------------------
InspectorPanel::InspectorPanel (AppContext& a) : app (a)
{
    content = std::make_unique<Content> (app, *this);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    app.session.addChangeListener (this);
}

InspectorPanel::~InspectorPanel()
{
    app.session.removeChangeListener (this);
}

void InspectorPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());
}

void InspectorPanel::resized()
{
    viewport.setBounds (getLocalBounds().withTrimmedLeft (1));
    content->setSize (viewport.getMaximumVisibleWidth(), content->contentHeight);
}

void InspectorPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content->refreshResult();
    content->setSize (viewport.getMaximumVisibleWidth(), content->getHeight());
}

void InspectorPanel::timerCallback()
{
    stopTimer();
    content->applyPendingSettings();
}
