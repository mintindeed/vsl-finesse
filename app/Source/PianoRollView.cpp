#include "PianoRollView.h"
#include "Theme.h"

PianoRollView::PianoRollView (AppContext& a) : app (a)
{
    app.session.addChangeListener (this);
    startTimerHz (30);
}

PianoRollView::~PianoRollView()
{
    app.session.removeChangeListener (this);
}

void PianoRollView::changeListenerCallback (juce::ChangeBroadcaster*) { repaint(); }

juce::Rectangle<int> PianoRollView::headerArea() const { return getLocalBounds().removeFromTop (34); }

juce::Rectangle<int> PianoRollView::notesArea() const
{
    auto r = getLocalBounds();
    r.removeFromTop (34);
    r.removeFromBottom (170);
    return r;
}

juce::Rectangle<int> PianoRollView::dynamicsArea() const
{
    auto r = getLocalBounds().removeFromBottom (170);
    return r.removeFromTop (110).reduced (0, 4);
}

juce::Rectangle<int> PianoRollView::expressionArea() const
{
    return getLocalBounds().removeFromBottom (60).reduced (0, 4);
}

void PianoRollView::zoomToFit()
{
    const double length = app.session.lengthSeconds();
    if (length > 0.0 && getWidth() > 0)
    {
        viewStart = 0.0;
        pixelsPerSecond = juce::jlimit (5.0, 800.0, (getWidth() - 20) / (length + 1.0));
    }
    repaint();
}

void PianoRollView::timerCallback()
{
    const float x = xForSeconds (app.engine.getPositionSeconds());
    if (std::abs (x - lastPlayheadX) < 0.5f)
        return;

    // Follow the play position.
    if (app.engine.isPlaying() && x > (float) getWidth() * 0.85f)
    {
        viewStart = app.engine.getPositionSeconds() - getWidth() * 0.15 / pixelsPerSecond;
        repaint();
    }
    else
    {
        repaint (juce::Rectangle<int> ((int) lastPlayheadX - 2, 0, 5, getHeight()));
        repaint (juce::Rectangle<int> ((int) x - 2, 0, 5, getHeight()));
    }
    lastPlayheadX = x;
}

void PianoRollView::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    auto* track = app.session.selectedTrack();
    if (track == nullptr || ! track->hasMidi)
    {
        g.setColour (theme::textDim);
        g.setFont (juce::FontOptions (18.0f));
        g.drawFittedText ("Drop a MIDI file here (one instrument per file)", getLocalBounds(), juce::Justification::centred, 2);
        return;
    }

    drawGrid (g, *track);
    drawNotes (g, *track);
    drawLane (g, *track, dynamicsArea(), track->profile.dynamicsCC, theme::dynamics,
              "CC" + juce::String (track->profile.dynamicsCC) + " dynamics");
    if (track->profile.expressionCC >= 0)
        drawLane (g, *track, expressionArea(), track->profile.expressionCC, theme::expression,
                  "CC" + juce::String (track->profile.expressionCC) + " expression");

    // Play head.
    const float x = xForSeconds (app.engine.getPositionSeconds());
    g.setColour (theme::playhead);
    g.drawLine (x, 0.0f, x, (float) getHeight(), 1.5f);
}

void PianoRollView::drawGrid (juce::Graphics& g, const Track& t)
{
    const auto& part = t.result.part;
    const auto& tempo = part.tempo;
    const auto header = headerArea();

    // Bars.
    std::vector<vslp::TimeSignature> meters = part.timeSignatures;
    if (meters.empty())
        meters.push_back ({ 0, 4, 4 });

    const double endSeconds = secondsForX ((float) getWidth());
    for (size_t m = 0; m < meters.size(); ++m)
    {
        const auto& ts = meters[m];
        const double barTicks = part.ppq * 4.0 * ts.numerator / juce::jmax (1, ts.denominator);
        const double until = m + 1 < meters.size() ? (double) meters[m + 1].tick : 1.0e12;
        int barNumber = 1;
        for (size_t k = 0; k < m; ++k)
        {
            const auto& prev = meters[k];
            const double prevBar = part.ppq * 4.0 * prev.numerator / juce::jmax (1, prev.denominator);
            barNumber += (int) std::ceil ((double) (meters[k + 1].tick - prev.tick) / prevBar);
        }

        // Skip label density when zoomed out.
        const double barSeconds = tempo.secondsAt ((double) ts.tick + barTicks) - tempo.secondsAt ((double) ts.tick);
        const int labelEvery = juce::jmax (1, (int) std::ceil (40.0 / (barSeconds * pixelsPerSecond)));

        for (double tick = (double) ts.tick; tick < until; tick += barTicks, ++barNumber)
        {
            const double s = tempo.secondsAt (tick);
            if (s > endSeconds)
                break;
            const float x = xForSeconds (s);
            if (x < -50.0f)
                continue;
            g.setColour (theme::outline.withAlpha (0.6f));
            g.drawVerticalLine ((int) x, (float) header.getBottom(), (float) getHeight());
            if ((barNumber - 1) % labelEvery == 0)
            {
                g.setColour (theme::textDim);
                g.setFont (juce::FontOptions (11.0f));
                g.drawText (juce::String (barNumber), (int) x + 3, header.getY() + 2, 40, 14, juce::Justification::left);
            }
        }
    }

    // Phrases.
    int index = 0;
    for (const auto& p : t.result.phrases)
    {
        const float x1 = xForSeconds (p.startSeconds);
        const float x2 = xForSeconds (p.endSeconds);
        if (x2 >= 0.0f && x1 <= (float) getWidth())
        {
            g.setColour (theme::accent.withAlpha (index % 2 == 0 ? 0.35f : 0.2f));
            g.fillRoundedRectangle (x1, (float) header.getY() + 18.0f, juce::jmax (2.0f, x2 - x1), 6.0f, 3.0f);
        }
        ++index;
    }

    // Keyswitches.
    g.setFont (juce::FontOptions (10.0f));
    for (const auto& ks : part.keyswitches)
    {
        const float x = xForSeconds (tempo.secondsAt ((double) ks.start));
        if (x < -40.0f || x > (float) getWidth())
            continue;
        g.setColour (theme::text.withAlpha (0.8f));
        g.fillRect (x, (float) header.getY() + 26.0f, 1.5f, 8.0f);
        g.drawText (vslp::noteName (ks.pitch), (int) x + 3, header.getY() + 24, 40, 10, juce::Justification::left);
    }
}

void PianoRollView::drawNotes (juce::Graphics& g, const Track& t)
{
    const auto area = notesArea();
    const auto& part = t.result.part;
    if (part.notes.empty())
        return;

    int lo = 127, hi = 0;
    for (const auto& n : part.notes)
    {
        lo = juce::jmin (lo, n.pitch);
        hi = juce::jmax (hi, n.pitch);
    }
    lo -= 2;
    hi += 2;
    const float rowHeight = juce::jlimit (3.0f, 16.0f, (float) area.getHeight() / (float) (hi - lo + 1));
    const float top = (float) area.getY() + ((float) area.getHeight() - rowHeight * (float) (hi - lo + 1)) * 0.5f;
    auto yFor = [&] (int pitch) { return top + (float) (hi - pitch) * rowHeight; };

    // Octave guides.
    g.setFont (juce::FontOptions (10.0f));
    for (int p = lo; p <= hi; ++p)
        if (p % 12 == 0)
        {
            g.setColour (theme::outline.withAlpha (0.4f));
            g.drawHorizontalLine ((int) (yFor (p) + rowHeight), (float) area.getX(), (float) area.getRight());
            g.setColour (theme::textDim);
            g.drawText (vslp::noteName (p), area.getX() + 2, (int) yFor (p), 30, (int) rowHeight, juce::Justification::left);
        }

    // Original notes as outlines, so timing changes are visible.
    const auto& inTempo = t.input.tempo;
    g.setColour (theme::original.withAlpha (0.7f));
    for (const auto& n : t.input.notes)
    {
        if (t.profile.isKeyswitch (n.pitch) || n.pitch < lo || n.pitch > hi)
            continue;
        const float x1 = xForSeconds (inTempo.secondsAt ((double) n.start));
        const float x2 = xForSeconds (inTempo.secondsAt ((double) n.end));
        if (x2 < 0.0f || x1 > (float) getWidth())
            continue;
        g.drawRect (juce::Rectangle<float> (x1, yFor (n.pitch), juce::jmax (1.0f, x2 - x1), rowHeight), 1.0f);
    }

    // Finessed notes.
    const auto& tempo = part.tempo;
    for (size_t i = 0; i < part.notes.size(); ++i)
    {
        const auto& n = part.notes[i];
        const float x1 = xForSeconds (tempo.secondsAt ((double) n.start));
        const float x2 = xForSeconds (tempo.secondsAt ((double) n.end));
        if (x2 < 0.0f || x1 > (float) getWidth())
            continue;
        const int art = i < t.result.articulationIndex.size() ? t.result.articulationIndex[i] : 0;
        const float brightness = 0.35f + 0.65f * (float) n.velocity / 127.0f;
        g.setColour (theme::articulation (art).withMultipliedBrightness (brightness).withAlpha (0.85f));
        g.fillRoundedRectangle (juce::Rectangle<float> (x1, yFor (n.pitch) + 1.0f, juce::jmax (2.0f, x2 - x1), rowHeight - 2.0f), 2.0f);
    }
}

void PianoRollView::drawLane (juce::Graphics& g, const Track& t, juce::Rectangle<int> area, int cc,
                              juce::Colour colour, const juce::String& label)
{
    g.setColour (theme::panel);
    g.fillRect (area);
    g.setColour (theme::outline);
    g.drawHorizontalLine (area.getY(), 0.0f, (float) getWidth());

    const auto& part = t.result.part;
    const auto& tempo = part.tempo;
    auto yFor = [&] (int v) { return (float) area.getBottom() - 2.0f - (float) v / 127.0f * (float) (area.getHeight() - 4); };

    juce::Path path;
    bool started = false;
    float lastY = 0.0f;
    for (const auto& c : part.controllers)
    {
        if (c.controller != cc)
            continue;
        const float x = xForSeconds (tempo.secondsAt ((double) c.tick));
        const float y = yFor (c.value);
        if (! started)
        {
            path.startNewSubPath (x, y);
            started = true;
        }
        else
        {
            path.lineTo (x, lastY); // controllers are steps
            path.lineTo (x, y);
        }
        lastY = y;
    }
    if (started)
    {
        path.lineTo ((float) getWidth() + 10.0f, lastY);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.6f));
    }

    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (label, area.reduced (6, 2), juce::Justification::topLeft);
}

void PianoRollView::mouseDown (const juce::MouseEvent& e)
{
    dragStartX = (float) e.x;
    dragStartView = viewStart;
    if (! e.mods.isPopupMenu() && ! e.mods.isAltDown())
        app.engine.setPosition (juce::jmax (0.0, secondsForX ((float) e.x)));
    repaint();
}

void PianoRollView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown()) // Alt-drag scrolls
    {
        viewStart = juce::jmax (-1.0, dragStartView - ((float) e.x - dragStartX) / pixelsPerSecond);
        repaint();
    }
    else
        app.engine.setPosition (juce::jmax (0.0, secondsForX ((float) e.x)));
}

void PianoRollView::zoomAround (float x, double factor)
{
    const double anchor = secondsForX (x);
    pixelsPerSecond = juce::jlimit (5.0, 800.0, pixelsPerSecond * factor);
    viewStart = anchor - x / pixelsPerSecond;
    repaint();
}

void PianoRollView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoomAround ((float) e.x, std::pow (1.15, w.deltaY * 6.0));
        return;
    }
    const float delta = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
    viewStart = juce::jmax (-1.0, viewStart - delta * 300.0 / pixelsPerSecond);
    repaint();
}

void PianoRollView::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    zoomAround ((float) e.x, scaleFactor);
}
