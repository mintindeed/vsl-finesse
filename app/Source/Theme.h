#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
inline const juce::Colour background   { 0xff1b1d22 };
inline const juce::Colour panel        { 0xff23262d };
inline const juce::Colour panelRaised  { 0xff2b2f37 };
inline const juce::Colour outline      { 0xff3a3f49 };
inline const juce::Colour text         { 0xffe6e8ec };
inline const juce::Colour textDim      { 0xff9aa1ad };
inline const juce::Colour accent       { 0xffe0a23b }; // finessed
inline const juce::Colour original     { 0xff7d8794 };
inline const juce::Colour dynamics     { 0xff5fb3f0 }; // CC1
inline const juce::Colour expression   { 0xff8bd17c }; // CC11
inline const juce::Colour playhead     { 0xffff5a5a };
inline const juce::Colour warning      { 0xfff0b35f };

// Articulation colours, cycled by index.
inline juce::Colour articulation (int index)
{
    static const juce::uint32 colours[] = { 0xffe0a23b, 0xff5fb3f0, 0xffc77dd8, 0xff8bd17c, 0xfff07a5f, 0xff5fd0c3, 0xffd8d87d };
    return juce::Colour (colours[(size_t) index % (sizeof (colours) / sizeof (colours[0]))]);
}
} // namespace theme
