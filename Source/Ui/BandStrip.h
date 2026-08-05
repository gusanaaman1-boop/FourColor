// The selected band's panel, mockup layout: a vertical COLOR radio list on the
// left, then DRIVE (big knob), BEHAVIOR (BODY <-> ATTACK gradient slider),
// TONE (DARK/BRIGHT), SPACE/SPREAD, and a compact band MIX. Everything rebinds
// when the selection changes; the panel border takes the band's colour.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Core/ParameterIds.h"
#include "Knob.h"

namespace fourcolor::ui
{
    class BandStrip : public juce::Component
    {
    public:
        BandStrip (juce::AudioProcessorValueTreeState& apvts);

        void setBand (int bandIndex);
        int getBand() const noexcept { return band; }

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void rebindColorButtons();

        juce::AudioProcessorValueTreeState& state;
        int band = 0;

        juce::TextButton colorButtons[4];
        std::unique_ptr<juce::ParameterAttachment> colorAttachment;
        int activeColor = 0;

        std::unique_ptr<Knob> drive, tone, space, bandMix;

        juce::Slider behaviorSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> behaviorAttachment;

        juce::Rectangle<int> colorArea, behaviorArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandStrip)
    };
}
