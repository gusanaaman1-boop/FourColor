#include "Knob.h"

namespace fourcolor::ui
{
    Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId,
                const juce::String& title, juce::Colour accent, const juce::String& tooltip)
        : state (apvts)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setVelocityModeParameters (1.0, 1, 0.09, true,
                                          juce::ModifierKeys::ctrlModifier);
        if (tooltip.isNotEmpty())
            slider.setTooltip (tooltip);
        addAndMakeVisible (slider);

        nameLabel.setText (title, juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setFont (uiFont (11.5f, true));
        nameLabel.setColour (juce::Label::textColourId, colour::textDim);
        nameLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (nameLabel);

        rebind (parameterId);
    }

    void Knob::rebind (const juce::String& parameterId)
    {
        attachment.reset();
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, parameterId, slider);
    }

    void Knob::resized()
    {
        auto area = getLocalBounds();
        nameLabel.setBounds (area.removeFromTop (14));
        slider.setBounds (area);
    }
}
