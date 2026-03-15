#pragma once

#include <JuceHeader.h>

namespace scroll_passthrough
{
inline void ignoreMouseWheel(const juce::MouseEvent&, const juce::MouseWheelDetails&) {}
} // namespace scroll_passthrough

class ScrollPassthroughViewport : public juce::Viewport
{
public:
    using juce::Viewport::Viewport;

    ~ScrollPassthroughViewport() override
    {
        detachMouseWheelListener();
    }

    void setViewedComponentWithMouseWheelPassthrough(juce::Component* newViewedComponent,
                                                     bool deleteComponentWhenNoLongerNeeded)
    {
        detachMouseWheelListener();
        juce::Viewport::setViewedComponent(newViewedComponent, deleteComponentWhenNoLongerNeeded);
        observedComponent = newViewedComponent;

        if (observedComponent != nullptr)
            observedComponent->addMouseListener(this, true);
    }

private:
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        juce::Viewport::mouseWheelMove(event.getEventRelativeTo(this), wheel);
    }

    void detachMouseWheelListener()
    {
        if (observedComponent != nullptr)
            observedComponent->removeMouseListener(this);

        observedComponent = nullptr;
    }

    juce::Component* observedComponent = nullptr;
};

class ScrollPassthroughSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        scroll_passthrough::ignoreMouseWheel(event, wheel);
    }
};

class ScrollPassthroughComboBox : public juce::ComboBox
{
public:
    using juce::ComboBox::ComboBox;

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        scroll_passthrough::ignoreMouseWheel(event, wheel);
    }
};

class ScrollPassthroughTextEditor : public juce::TextEditor
{
public:
    using juce::TextEditor::TextEditor;

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        scroll_passthrough::ignoreMouseWheel(event, wheel);
    }
};
