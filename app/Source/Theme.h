#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The app's colours: the "wine" palette. Every colour the app draws comes from
// here, and JUCE's own widgets (buttons, menus, sliders, tooltips) get theirs
// from colourScheme(), which Main.cpp installs. The comments name the palette's
// tokens; colours the palette doesn't have say what they're made from.
namespace theme
{
// Surfaces, darkest first.
inline const juce::Colour background   { 0xff160a10 }; // bg.base: window, piano roll
inline const juce::Colour panel        { 0xff1e1017 }; // bg.surface: toolbar, lanes, inspector
inline const juce::Colour panelRaised  { 0xff2a1620 }; // bg.raised: selected track, buttons, fields, menus
inline const juce::Colour outline      { 0xff3d2430 }; // border
inline const juce::Colour grid         { 0xff2b1922 }; // grid.line: bar lines, octave guides

inline const juce::Colour text         { 0xfff6e7dc }; // text.primary
inline const juce::Colour textDim      { 0xffb8a1a8 }; // text.secondary

// Selection. `selected` fills (toggled buttons, the selected bars, menu hover);
// it's too dark to read as a thin line, so lines and small marks use `highlight`,
// a lighter wine (not in the palette).
inline const juce::Colour selected     { 0xff7a2440 }; // accent.selected
inline const juce::Colour highlight    { 0xffc24d6b };
inline const juce::Colour brand        { 0xff4a1428 }; // brand.wine: the Welcome screen's button, the app icon

inline const juce::Colour noteFill     { 0xfff0c4a2 }; // note.fill: the first articulation's notes, solo
inline const juce::Colour original     = textDim;      // outlines of the notes as they came in
inline const juce::Colour dynamics     { 0xffff9aa8 }; // cc1.line
inline const juce::Colour expression   { 0xff9db7ff }; // cc11.line
inline const juce::Colour playhead     { 0xffffe9d6 }; // playhead
inline const juce::Colour warning      { 0xfff09a4e }; // orange (not in the palette): a pick the preset doesn't have

// Articulation colours, cycled by index: note.fill first, then soft colours of
// about the same brightness, kept away from the CC1 pink and CC11 blue.
inline juce::Colour articulation (int index)
{
    static const juce::uint32 colours[] = { 0xfff0c4a2, 0xff92c6e8, 0xffa6d4a0, 0xffc4a8ec, 0xffe4d68e, 0xff8ed4cc, 0xffe4a0d0 };
    return juce::Colour (colours[(size_t) index % (sizeof (colours) / sizeof (colours[0]))]);
}

// For JUCE's LookAndFeel_V4: window, widget, menu, outline, text, fill
// (slider thumbs, scroll bars, text cursor), highlighted text, highlighted fill
// (toggled buttons, menu hover, slider tracks, tooltips), menu text.
inline juce::LookAndFeel_V4::ColourScheme colourScheme()
{
    return { background, panelRaised, panelRaised,
             outline, text, highlight,
             text, selected, text };
}
} // namespace theme
