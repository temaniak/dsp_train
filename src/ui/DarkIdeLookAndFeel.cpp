#include "ui/DarkIdeLookAndFeel.h"
#include "ui/ScrollPassthroughControls.h"

namespace ide
{
namespace
{
class ScrollPassthroughLabel final : public juce::Label
{
public:
    ScrollPassthroughLabel() : juce::Label({}, {}) {}

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        scroll_passthrough::ignoreMouseWheel(event, wheel);
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return createIgnoredAccessibilityHandler(*this);
    }
};
} // namespace

DarkIdeLookAndFeel::DarkIdeLookAndFeel()
{
    auto scheme = juce::LookAndFeel_V4::getDarkColourScheme();
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::windowBackground, background);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::widgetBackground, panel);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::menuBackground, panel);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::outline, border);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::defaultText, text);
    setColourScheme(scheme);

    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::headerTextColourId, textSecondary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, active);
    setColour(juce::PopupMenu::highlightedTextColourId, text);
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::buttonColourId, panel);
    setColour(juce::ComboBox::arrowColourId, textSecondary);
    setColour(juce::ComboBox::focusedOutlineColourId, constant);
    setColour(juce::TextButton::buttonColourId, active);
    setColour(juce::TextButton::buttonOnColourId, active.brighter(0.15f));
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, text);
    setColour(juce::Label::textColourId, text);
    setColour(juce::TextEditor::backgroundColourId, panel);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::highlightColourId, active);
    setColour(juce::TextEditor::highlightedTextColourId, text);
    setColour(juce::TextEditor::outlineColourId, border);
    setColour(juce::TextEditor::focusedOutlineColourId, constant);
    setColour(juce::TreeView::backgroundColourId, panel);
    setColour(juce::TreeView::linesColourId, border);
    setColour(juce::ScrollBar::backgroundColourId, panel);
    setColour(juce::ScrollBar::thumbColourId, border.brighter(0.25f));
    setColour(juce::AlertWindow::backgroundColourId, panel);
    setColour(juce::AlertWindow::textColourId, text);
    setColour(juce::AlertWindow::outlineColourId, border);
    setColour(juce::DocumentWindow::textColourId, text);
}

juce::Font DarkIdeLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(juce::jlimit(13.0f, 15.5f, buttonHeight * 0.45f), juce::Font::plain));
}

juce::Font DarkIdeLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(14.0f, juce::Font::plain));
}

void DarkIdeLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                              juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto colour = backgroundColour;

    if (! button.isEnabled())
        colour = panel.darker(0.15f);
    else if (shouldDrawButtonAsDown)
        colour = active.brighter(0.18f);
    else if (shouldDrawButtonAsHighlighted)
        colour = active.brighter(0.1f);

    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
}

void DarkIdeLookAndFeel::drawComboBox(juce::Graphics& g,
                                      int width,
                                      int height,
                                      bool isButtonDown,
                                      int buttonX,
                                      int buttonY,
                                      int buttonW,
                                      int buttonH,
                                      juce::ComboBox& box)
{
    juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);

    const auto bounds = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    g.setColour(isButtonDown ? active.brighter(0.06f) : box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(box.findColour(box.hasKeyboardFocus(true) ? juce::ComboBox::focusedOutlineColourId
                                                          : juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    juce::Path arrow;
    const auto centreX = static_cast<float>(width - 18);
    const auto centreY = static_cast<float>(height / 2);
    arrow.startNewSubPath(centreX - 4.0f, centreY - 2.0f);
    arrow.lineTo(centreX, centreY + 2.0f);
    arrow.lineTo(centreX + 4.0f, centreY - 2.0f);

    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(arrow, juce::PathStrokeType(1.5f));
}

juce::Label* DarkIdeLookAndFeel::createComboBoxTextBox(juce::ComboBox&)
{
    return new ScrollPassthroughLabel();
}

void DarkIdeLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                           const juce::Rectangle<int>& area,
                                           bool isSeparator,
                                           bool isActive,
                                           bool isHighlighted,
                                           bool isTicked,
                                           bool hasSubMenu,
                                           const juce::String& textToDraw,
                                           const juce::String& shortcutKeyText,
                                           const juce::Drawable* icon,
                                           const juce::Colour* textColour)
{
    juce::ignoreUnused(isTicked, hasSubMenu, shortcutKeyText, icon);

    if (isSeparator)
    {
        g.setColour(border);
        g.drawHorizontalLine(area.getCentreY(), static_cast<float>(area.getX() + 8), static_cast<float>(area.getRight() - 8));
        return;
    }

    if (isHighlighted)
    {
        g.setColour(active);
        g.fillRoundedRectangle(area.toFloat().reduced(4.0f, 2.0f), 4.0f);
    }

    g.setColour(textColour != nullptr ? *textColour : (isActive ? text : textMuted));
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::plain)));
    g.drawText(textToDraw, area.reduced(12, 0), juce::Justification::centredLeft, true);
}

void DarkIdeLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                          int x,
                                          int y,
                                          int width,
                                          int height,
                                          float sliderPosProportional,
                                          float rotaryStartAngle,
                                          float rotaryEndAngle,
                                          juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x),
                                         static_cast<float>(y),
                                         static_cast<float>(width),
                                         static_cast<float>(height)).reduced(10.0f, 8.0f);

    const auto radius = 0.5f * juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto strokeThickness = juce::jmax(1.1f, radius * 0.07f);
    const auto pointerThickness = juce::jmax(1.2f, radius * 0.075f);
    const auto arcRadius = radius - strokeThickness * 1.4f;
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centre.x,
                                centre.y,
                                arcRadius,
                                arcRadius,
                                0.0f,
                                rotaryStartAngle,
                                rotaryEndAngle,
                                true);

    g.setColour(border.brighter(0.08f));
    g.strokePath(backgroundArc, juce::PathStrokeType(strokeThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x,
                           centre.y,
                           arcRadius,
                           arcRadius,
                           0.0f,
                           rotaryStartAngle,
                           angle,
                           true);

    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(valueArc, juce::PathStrokeType(strokeThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path pointer;
    pointer.startNewSubPath(0.0f, -arcRadius + strokeThickness * 0.65f);
    pointer.lineTo(0.0f, -arcRadius * 0.28f);

    g.setColour(text);
    g.strokePath(pointer,
                 juce::PathStrokeType(pointerThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    g.setColour(active.brighter(0.06f));
    g.fillEllipse(centre.x - radius * 0.10f, centre.y - radius * 0.10f, radius * 0.20f, radius * 0.20f);
}

juce::Label* DarkIdeLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = new ScrollPassthroughLabel();
    label->setJustificationType(juce::Justification::centred);
    label->setKeyboardType(juce::TextInputTarget::decimalKeyboard);

    label->setColour(juce::Label::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
    label->setColour(juce::Label::backgroundColourId,
                     (slider.getSliderStyle() == juce::Slider::LinearBar || slider.getSliderStyle() == juce::Slider::LinearBarVertical)
                         ? juce::Colours::transparentBlack
                         : slider.findColour(juce::Slider::textBoxBackgroundColourId));
    label->setColour(juce::Label::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
    label->setColour(juce::TextEditor::textColourId, slider.findColour(juce::Slider::textBoxTextColourId));
    label->setColour(juce::TextEditor::backgroundColourId,
                     slider.findColour(juce::Slider::textBoxBackgroundColourId)
                         .withAlpha((slider.getSliderStyle() == juce::Slider::LinearBar || slider.getSliderStyle() == juce::Slider::LinearBarVertical)
                                        ? 0.7f
                                        : 1.0f));
    label->setColour(juce::TextEditor::outlineColourId, slider.findColour(juce::Slider::textBoxOutlineColourId));
    label->setColour(juce::TextEditor::highlightColourId, slider.findColour(juce::Slider::textBoxHighlightColourId));
    return label;
}

void DarkIdeLookAndFeel::drawStretchableLayoutResizerBar(juce::Graphics& g,
                                                         int width,
                                                         int height,
                                                         bool isVerticalBar,
                                                         bool isMouseOver,
                                                         bool isMouseDragging)
{
    g.fillAll(background);

    auto gripBounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))
                          .reduced(isVerticalBar ? 1.5f : 8.0f,
                                   isVerticalBar ? 8.0f : 1.5f);
    const auto gripColour = isMouseDragging ? constant
                          : isMouseOver    ? textSecondary
                                           : border;

    g.setColour(gripColour);
    g.fillRoundedRectangle(gripBounds, 3.0f);
}
} // namespace ide
