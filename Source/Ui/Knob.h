// A labelled rotary bound to one APVTS parameter. Double-click resets to the
// parameter default (JUCE built-in), ctrl/cmd-drag gives fine control via
// velocity mode, and the current value reads out under the knob.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Theme.h"

namespace fourcolor::ui
{
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterId,
              const juce::String& title, juce::Colour accent, const juce::String& tooltip = {});

        void resized() override;

        //  Rebind to a different parameter (used when the selected band changes).
        void rebind (const juce::String& parameterId);

        juce::Slider& getSlider() noexcept { return slider; }

    private:
        juce::AudioProcessorValueTreeState& state;
        juce::Slider slider;
        juce::Label nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
    };
}
