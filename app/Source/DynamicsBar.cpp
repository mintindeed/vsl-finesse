#include "DynamicsBar.h"
#include "Bars.h"
#include "Theme.h"

DynamicsBar::DynamicsBar (AppContext& a) : app (a)
{
    addAndMakeVisible (scopeLabel);
    scopeLabel.setFont (juce::FontOptions (13.0f));
    scopeLabel.setColour (juce::Label::textColourId, theme::textDim);

    for (int level = 0; level < vslp::numDynamicLevels; ++level)
    {
        auto* b = dynamicButtons.add (new juce::TextButton (vslp::dynamicSymbol (level)));
        b->setTooltip (juce::String (vslp::dynamicSymbol (level)) + ": " + vslp::dynamicMeaning (level));
        b->setConnectedEdges ((level > 0 ? juce::Button::ConnectedOnLeft : 0)
                              | (level < vslp::numDynamicLevels - 1 ? juce::Button::ConnectedOnRight : 0));
        b->onClick = [this, level] { apply (vslp::Marking::Kind::Dynamic, level); };
        addAndMakeVisible (b);
    }

    crescButton.setTooltip ("cresc.: crescendo, gradually louder (two steps), then stays there until the next marking");
    dimButton.setTooltip ("dim.: diminuendo (decrescendo), gradually softer (two steps), then stays there until the next marking");
    clearButton.setTooltip ("Remove the dynamics markings here");
    crescButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    dimButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    crescButton.onClick = [this] { apply (vslp::Marking::Kind::Crescendo, 0); };
    dimButton.onClick = [this] { apply (vslp::Marking::Kind::Diminuendo, 0); };
    clearButton.onClick = [this] {
        if (auto* t = app.session.selectedTrack())
            app.clearMarkings (*t);
    };
    for (auto* b : { &crescButton, &dimButton, &clearButton })
        addAndMakeVisible (b);

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
    for (auto* b : { &crescButton, &dimButton })
        b->setEnabled (enabled);
    clearButton.setEnabled (enabled && ! t->markings.empty());

    juce::String scope = "Dynamics for the whole part";
    if (enabled && app.session.hasSelection())
    {
        const int first = barAt (t->input, app.session.selectionStart);
        const int last = barAt (t->input, app.session.selectionEnd - 1);
        scope = first == last ? "Dynamics for bar " + juce::String (first)
                              : "Dynamics for bars " + juce::String (first) + "-" + juce::String (last);
    }
    else if (enabled)
        scope << "  (drag across bars to pick a passage)";
    scopeLabel.setText (enabled ? scope : juce::String ("Dynamics"), juce::dontSendNotification);
}

void DynamicsBar::apply (vslp::Marking::Kind kind, int level)
{
    if (auto* t = app.session.selectedTrack())
    {
        vslp::Marking m;
        m.kind = kind;
        m.level = level;
        app.applyMarking (*t, m);
    }
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
    scopeLabel.setBounds (r);
}
