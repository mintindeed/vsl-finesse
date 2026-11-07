#include "DynamicsBar.h"
#include "Bars.h"
#include "Theme.h"

DynamicsBar::DynamicsBar (AppContext& a) : app (a)
{
    addAndMakeVisible (scopeLabel);
    scopeLabel.setFont (juce::FontOptions (13.0f));
    scopeLabel.setColour (juce::Label::textColourId, theme::textDim);

    for (int level = 0; level < vslf::numDynamicLevels; ++level)
    {
        auto* b = dynamicButtons.add (new juce::TextButton (vslf::dynamicSymbol (level)));
        b->setTooltip (juce::String (vslf::dynamicSymbol (level)) + ": " + vslf::dynamicMeaning (level));
        b->setConnectedEdges ((level > 0 ? juce::Button::ConnectedOnLeft : 0)
                              | (level < vslf::numDynamicLevels - 1 ? juce::Button::ConnectedOnRight : 0));
        b->onClick = [this, level] { apply (vslf::Marking::Kind::Dynamic, level); };
        addAndMakeVisible (b);
    }

    crescButton.setTooltip ("cresc.: crescendo, gradually louder (two steps), then stays there until the next marking");
    dimButton.setTooltip ("dim.: diminuendo (decrescendo), gradually softer (two steps), then stays there until the next marking");
    clearButton.setTooltip ("Remove the dynamics markings here");
    crescButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    dimButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    crescButton.onClick = [this] { apply (vslf::Marking::Kind::Crescendo, 0); };
    dimButton.onClick = [this] { apply (vslf::Marking::Kind::Diminuendo, 0); };
    clearButton.onClick = [this] {
        if (auto* t = app.session.selectedTrack())
            app.clearMarkings (*t);
    };
    for (auto* b : { &crescButton, &dimButton, &clearButton })
        addAndMakeVisible (b);

    articulationButton.setTooltip ("Pick an articulation for the selected bars (or the whole part). "
                                   "The app writes the keyswitches that select it.");
    articulationButton.onClick = [this] { showArticulationMenu(); };
    addAndMakeVisible (articulationButton);

    app.session.addChangeListener (this);
    refresh();
}

DynamicsBar::~DynamicsBar()
{
    app.session.removeChangeListener (this);
}

void DynamicsBar::changeListenerCallback (juce::ChangeBroadcaster*) { refresh(); }

void DynamicsBar::refresh()
{
    auto* t = app.session.selectedTrack();
    const bool enabled = t != nullptr && t->hasMidi;
    for (auto* b : dynamicButtons)
        b->setEnabled (enabled);
    for (auto* b : { &crescButton, &dimButton, &articulationButton })
        b->setEnabled (enabled);
    clearButton.setEnabled (enabled && ! t->markings.empty());

    juce::String scope = "Whole part";
    if (enabled && app.session.hasSelection())
    {
        const int first = barAt (t->input, app.session.selectionStart);
        const int last = barAt (t->input, app.session.selectionEnd - 1);
        scope = first == last ? "Bar " + juce::String (first)
                              : "Bars " + juce::String (first) + "-" + juce::String (last);
    }
    else if (enabled)
        scope << "  (drag across bars to pick a passage)";
    scopeLabel.setText (enabled ? scope : juce::String ("Articulations and dynamics"), juce::dontSendNotification);
}

void DynamicsBar::apply (vslf::Marking::Kind kind, int level)
{
    if (auto* t = app.session.selectedTrack())
    {
        vslf::Marking m;
        m.kind = kind;
        m.level = level;
        app.applyMarking (*t, m);
    }
}

void DynamicsBar::showArticulationMenu()
{
    auto* t = app.session.selectedTrack();
    if (t == nullptr || ! t->hasMidi)
        return;

    struct Choice
    {
        juce::String name;
        std::vector<int> keys;
    };
    auto choices = std::make_shared<std::vector<Choice>>();
    auto keysText = [] (const std::vector<int>& keys) {
        juce::StringArray names;
        for (int k : keys)
            names.add (vslf::noteName (k));
        return names.joinIntoString (" + ");
    };
    auto item = [&] (const juce::String& label, const juce::String& name, const std::vector<int>& keys) {
        choices->push_back ({ name, keys });
        juce::PopupMenu::Item i (label);
        i.itemID = (int) choices->size();
        i.shortcutKeyDescription = keysText (keys);
        return i;
    };

    juce::PopupMenu menu;
    if (! t->synchronArticulations.empty())
    {
        // The preset's tree: "Long notes > Marcato" is Marcato in a Long notes submenu.
        menu.addSectionHeader ("From the Synchron preset");
        juce::String openGroup;
        juce::PopupMenu group;
        auto flush = [&] {
            if (openGroup.isNotEmpty())
                menu.addSubMenu (openGroup, group);
            openGroup.clear();
            group = {};
        };
        for (const auto& a : t->synchronArticulations)
        {
            if (a.path.size() == 1)
            {
                flush();
                menu.addItem (item (a.path[0], a.name(), a.keys));
                continue;
            }
            if (a.path[0] != openGroup)
            {
                flush();
                openGroup = a.path[0];
            }
            juce::StringArray rest (a.path);
            rest.remove (0);
            group.addItem (item (rest.joinIntoString (" > "), a.name(), a.keys));
        }
        flush();
    }
    else
    {
        // No tree to read: the profile's named articulations and the key
        // combinations in the file.
        bool any = false;
        for (const auto& m : t->profile.articulations)
            if (! m.keys.empty())
            {
                menu.addItem (item (m.articulation.name, m.articulation.name, m.keys));
                any = true;
            }
        for (const auto& u : t->result.report.articulations)
            if (! u.keys.empty() && ! u.mapped)
            {
                if (! any)
                    menu.addSectionHeader ("Keyswitches in the file");
                menu.addItem (item (keysText (u.keys), keysText (u.keys), u.keys));
                any = true;
            }
        if (! any)
            menu.addItem (juce::PopupMenu::Item ("Load a Synchron Player preset to pick articulations by name").setEnabled (false));
    }

    menu.addSeparator();
    constexpr int clearId = 100000;
    menu.addItem (juce::PopupMenu::Item (app.session.hasSelection() ? "Remove picked articulations here"
                                                                     : "Remove all picked articulations")
                      .setID (clearId)
                      .setEnabled (! t->articulationMarkings.empty()));

    juce::Component::SafePointer<DynamicsBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&articulationButton),
                        [safe, choices] (int result) {
                            if (safe == nullptr || result == 0)
                                return;
                            auto* track = safe->app.session.selectedTrack();
                            if (track == nullptr)
                                return;
                            if (result == clearId)
                            {
                                safe->app.clearArticulations (*track);
                                return;
                            }
                            if (! juce::isPositiveAndNotGreaterThan (result, (int) choices->size()))
                                return;
                            const auto& c = (*choices)[(size_t) result - 1];
                            vslf::ArticulationMarking m;
                            m.name = c.name.toStdString();
                            m.keys = c.keys;
                            safe->app.applyArticulation (*track, m);
                        });
}

void DynamicsBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void DynamicsBar::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    clearButton.setBounds (r.removeFromRight (60));
    r.removeFromRight (12);
    dimButton.setBounds (r.removeFromRight (56));
    crescButton.setBounds (r.removeFromRight (56));
    r.removeFromRight (12);
    for (int i = dynamicButtons.size(); --i >= 0;)
        dynamicButtons[i]->setBounds (r.removeFromRight (40));
    r.removeFromRight (12);
    articulationButton.setBounds (r.removeFromRight (110));
    r.removeFromRight (12);
    scopeLabel.setBounds (r);
}
