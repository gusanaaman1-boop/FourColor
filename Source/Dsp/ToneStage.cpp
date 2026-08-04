#include "ToneStage.h"

namespace fourcolor
{
    void ToneStage::prepare (double sampleRate, int numChannels)
    {
        rate = sampleRate;
        channels = juce::jlimit (1, 2, numChannels);
        currentCentre = 0.0f;
        reset();
    }

    void ToneStage::reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            preHp[c].reset();
            postHp[c].reset();
        }
    }

    void ToneStage::setTone (float tone, float centreHz) noexcept
    {
        tone = juce::jlimit (-1.0f, 1.0f, tone);

        //  +/-9 dB net tilt, split as two equal sqrt halves around the shaper.
        const float netGain  = std::pow (10.0f, tone * 9.0f / 20.0f);
        const float half     = std::sqrt (netGain) - 1.0f;

        amountPre  = half;
        amountPost = half;

        //  Retune the shelf corner only when the centre actually moves
        //  (crossover automation); a 5% threshold avoids per-block tan().
        if (std::abs (centreHz - currentCentre) > 0.05f * juce::jmax (1.0f, currentCentre))
        {
            currentCentre = centreHz;
            for (int c = 0; c < 2; ++c)
            {
                preHp[c].setCutoff (rate, centreHz);
                postHp[c].setCutoff (rate, centreHz);
            }
        }
    }

    void ToneStage::apply (juce::AudioBuffer<float>& buffer, dsp::OnePole* filters, float amount) noexcept
    {
        //  y = x + amount * highpass(x): a first-order high shelf of gain
        //  (1 + amount) above the corner.
        const int n = buffer.getNumSamples();

        for (int c = 0; c < juce::jmin (buffer.getNumChannels(), channels); ++c)
        {
            auto* d = buffer.getWritePointer (c);
            auto& f = filters[c];

            for (int i = 0; i < n; ++i)
                d[i] += amount * f.processHigh (d[i]);
        }
    }

    void ToneStage::processPre (juce::AudioBuffer<float>& buffer) noexcept
    {
        //  The filter must keep running even at amount 0 so engaging the tone
        //  is stateless-transition free.
        apply (buffer, preHp, amountPre);
    }

    void ToneStage::processPost (juce::AudioBuffer<float>& buffer) noexcept
    {
        apply (buffer, postHp, amountPost);
    }
}
