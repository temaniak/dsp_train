#pragma once

#include <JuceHeader.h>

namespace ide
{
inline const juce::Colour background { 0xff1e1e1e };
inline const juce::Colour panel { 0xff252526 };
inline const juce::Colour active { 0xff2a2d2e };
inline const juce::Colour border { 0xff3c3c3c };
inline const juce::Colour text { 0xffd4d4d4 };
inline const juce::Colour textSecondary { 0xffa0a0a0 };
inline const juce::Colour textMuted { 0xff6a6a6a };
inline const juce::Colour keyword { 0xffc586c0 };
inline const juce::Colour type { 0xff4ec9b0 };
inline const juce::Colour function { 0xffdcdcaa };
inline const juce::Colour variable { 0xff9cdcfe };
inline const juce::Colour constant { 0xff4fc1ff };
inline const juce::Colour string { 0xffce9178 };
inline const juce::Colour number { 0xffb5cea8 };
inline const juce::Colour comment { 0xff6a9955 };
inline const juce::Colour error { 0xfff44747 };
inline const juce::Colour warning { 0xffffcc66 };
inline const juce::Colour success { 0xff608b4e };

class DarkIdeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DarkIdeLookAndFeel();

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool isButtonDown,
                      int buttonX,
                      int buttonY,
                      int buttonW,
                      int buttonH,
                      juce::ComboBox& box) override;
    juce::Label* createComboBoxTextBox(juce::ComboBox&) override;

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool isTicked,
                           bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override;

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;

    void drawStretchableLayoutResizerBar(juce::Graphics& g,
                                         int width,
                                         int height,
                                         bool isVerticalBar,
                                         bool isMouseOver,
                                         bool isMouseDragging) override;
};
} // namespace ide
