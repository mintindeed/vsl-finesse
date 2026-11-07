#include "WelcomeOverlay.h"
#include "Theme.h"

namespace
{
struct Section
{
    juce::String heading, body;
    bool highlight = false;
};

std::vector<Section> sections (bool synchronInstalled)
{
    std::vector<Section> s;

    s.push_back ({ "Made for VSL Synchron Player",
                   "VSL Finesse was designed to play through Vienna Symphonic Library's Synchron Player and the Studio "
                   "Special Editions. Synchron Player loads on a track by itself when you open a MIDI file, and the app "
                   "follows the preset you load in it. You can pick other virtual instruments from a track's instrument "
                   "button, but they may not behave as intended." });
    if (! synchronInstalled)
        s.push_back ({ "Synchron Player isn't installed",
                       "Please install it: Vienna Assistant installs it with your Vienna libraries. Until then you can open and finesse MIDI files, but you won't hear them through "
                       "Synchron Player.",
                       true });

    s.push_back ({ "Why macOS asks for permissions",
                   "Instrument plug-ins such as Synchron Player run inside VSL Finesse, so macOS asks about anything "
                   "they do in VSL Finesse's name. You only get asked once. You can change your answers later in "
                   "System Settings > Privacy & Security." });
    s.push_back ({ "Files on a removable volume",
                   "Synchron Player reads its samples from wherever your Vienna library is installed, which is often an "
                   "external drive. Allow this, or instruments on that drive won't load and play silence." });
    s.push_back ({ "Files in your Documents folder",
                   "New sessions are saved in Documents unless you choose another folder, and Synchron Player may keep "
                   "presets and settings there. Allow this to save and open sessions and presets there." });
    s.push_back ({ "Find devices on your local network",
                   "VSL Finesse doesn't use the network. Vienna's software asks for this when Synchron Player loads. "
                   "You can deny it; if something Vienna-related then stops working, allow it in System Settings." });
    s.push_back ({ "Receive keystrokes from any app",
                   "VSL Finesse only reads the keyboard when its own window is in front (space plays, T switches "
                   "Original and Finessed). The request comes from a plug-in running inside it. VSL Finesse doesn't "
                   "need it, so you can deny it." });
    return s;
}
} // namespace

// The card's text, laid out to the viewport's width.
class WelcomeOverlay::Text : public juce::Component
{
public:
    explicit Text (bool synchronInstalled) : content (sections (synchronInstalled)) {}

    void layoutForWidth (int width)
    {
        layouts.clear();
        int y = 0;
        for (const auto& s : content)
        {
            juce::AttributedString a;
            a.append (s.heading + "\n", juce::FontOptions (16.0f, juce::Font::bold), s.highlight ? theme::warning : theme::text);
            a.append (s.body, juce::FontOptions (14.5f), s.highlight ? theme::warning.brighter (0.4f) : theme::textDim);
            a.setLineSpacing (2.0f);
            a.setWordWrap (juce::AttributedString::byWord);

            juce::TextLayout layout;
            layout.createLayout (a, (float) (width - 2 * pad));
            const int h = (int) std::ceil (layout.getHeight());
            layouts.push_back ({ std::move (layout), y, h, s.highlight });
            y += h + 18;
        }
        setSize (width, juce::jmax (0, y - 18));
    }

    void paint (juce::Graphics& g) override
    {
        for (const auto& l : layouts)
        {
            const auto area = juce::Rectangle<float> ((float) pad, (float) l.y, (float) (getWidth() - 2 * pad), (float) l.height);
            if (l.highlight)
            {
                g.setColour (theme::warning.withAlpha (0.12f));
                g.fillRoundedRectangle (area.expanded ((float) pad, 6.0f), 6.0f);
            }
            l.layout.draw (g, area);
        }
    }

private:
    struct Laid
    {
        juce::TextLayout layout;
        int y, height;
        bool highlight;
    };

    static constexpr int pad = 10; // room for the highlight around a section

    std::vector<Section> content;
    std::vector<Laid> layouts;
};

WelcomeOverlay::WelcomeOverlay (bool synchronInstalled) : text (std::make_unique<Text> (synchronInstalled))
{
    setWantsKeyboardFocus (true);
    viewport.setViewedComponent (text.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    okButton.setColour (juce::TextButton::buttonColourId, theme::accent.darker (0.3f));
    okButton.onClick = [this] {
        if (onDismiss)
            onDismiss();
    };
    addAndMakeVisible (okButton);
}

WelcomeOverlay::~WelcomeOverlay() = default;

juce::Rectangle<int> WelcomeOverlay::cardBounds() const
{
    const int w = juce::jmin (640, getWidth() - 40);
    const int h = juce::jmin (720, getHeight() - 40);
    return getLocalBounds().withSizeKeepingCentre (w, h);
}

void WelcomeOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));

    const auto card = cardBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (card, 10.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (card.reduced (0.5f), 10.0f, 1.0f);

    auto top = cardBounds().reduced (28, 0).withTrimmedTop (22).withHeight (34);
    g.setColour (theme::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("Welcome to VSL Finesse", top, juce::Justification::centredLeft);
}

void WelcomeOverlay::resized()
{
    auto card = cardBounds().reduced (18, 0);
    card.removeFromTop (70);
    auto bottom = card.removeFromBottom (64);
    okButton.setBounds (bottom.withSizeKeepingCentre (120, 32).withX (bottom.getRight() - 130));
    viewport.setBounds (card);
    text->layoutForWidth (card.getWidth() - viewport.getScrollBarThickness() - 4);
}

bool WelcomeOverlay::keyPressed (const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey) && onDismiss)
        onDismiss();
    return true; // keys don't reach the window behind (space would start playback)
}
