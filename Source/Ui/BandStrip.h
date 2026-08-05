// The selected band's controls - the only band whose full control set is ever
// on screen. Colour selector, big Drive, Behavior, Tone, Space, Band Mix and
// Band Level; everything rebinds when the selection changes.

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

        juce::Label header;
        juce::TextButton colorButtons[4];
        std::unique_ptr<juce::ParameterAttachment> colorAttachment;

        std::unique_ptr<Knob> drive, behavior, tone, space, bandMix, level;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandStrip)
    };
}
