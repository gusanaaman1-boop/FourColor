// Per-band DARK <-> BRIGHT control. Not a post-EQ: the tilt is split equally
// between a pre-emphasis half (which changes WHAT the shaper distorts) and a
// post half (which finishes the tonal move and reins in harsh top on high
// drive). Total reach is +/-9 dB of high-shelf at the band's centre, so a
// band's tone cannot dump energy far outside its own region.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "TptFilters.h"

namespace fourcolor
{
    class ToneStage
    {
    public:
        ToneStage() = default;

        void prepare (double sampleRate, int numChannels);
        void reset();

        //  tone in [-1, 1], centreHz = the band's geometric centre. Cheap.
        void setTone (float tone, float centreHz) noexcept;

        void processPre  (juce::AudioBuffer<float>& buffer) noexcept;
        void processPost (juce::AudioBuffer<float>& buffer) noexcept;

    private:
        void apply (juce::AudioBuffer<float>& buffer, dsp::OnePole* filters, float amount) noexcept;

        double rate = 48000.0;
        int channels = 2;
        float amountPre = 0.0f, amountPost = 0.0f;
        float currentCentre = 0.0f;

        dsp::OnePole preHp[2], postHp[2];

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneStage)
    };
}
