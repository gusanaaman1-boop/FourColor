#include "ToneStage.h"

namespace fourcolor
{
    void ToneStage::prepare (double sampleRate, int numChannels)
    {
        rate = sampleRate;
        channels = juce::jlimit (1, 2, numChannels);
        currentCentre = 0.0f;

        //  20 ms: fast enough that a Tone move feels immediate, long enough
        //  that the largest jump the control allows cannot step.
        amountPre.reset (sampleRate, 0.02);
        amountPost.reset (sampleRate, 0.02);

        reset();
    }

    void ToneStage::reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            preHp[c].reset();
            postHp[c].reset();
        }

        //  A reset is not a parameter move: land on the current target rather
        //  than gliding to it from wherever the smoother happened to be.
        amountPre.setCurrentAndTargetValue (amountPre.getTargetValue());
        amountPost.setCurrentAndTargetValue (amountPost.getTargetValue());
    }

    void ToneStage::setTone (float tone, float centreHz) noexcept
    {
        tone = juce::jlimit (-1.0f, 1.0f, tone);

        //  +/-9 dB net tilt, split as two equal sqrt halves around the shaper.
        const float netGain  = std::pow (10.0f, tone * 9.0f / 20.0f);
        const float half     = std::sqrt (netGain) - 1.0f;

        amountPre.setTargetValue (half);
        amountPost.setTargetValue (half);

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

    void ToneStage::apply (juce::AudioBuffer<float>& buffer, dsp::OnePole* filters,
                           juce::SmoothedValue<float>& amount) noexcept
    {
        //  y = x + amount * highpass(x): a first-order high shelf of gain
        //  (1 + amount) above the corner.
        //
        //  Samples outer, channels inner, so the smoother advances exactly once
        //  per sample and both channels are given the SAME gain. Advancing it
        //  per channel would both halve the ramp time and lean the image.
        const int n = buffer.getNumSamples();
        const int chans = juce::jmin (buffer.getNumChannels(), channels);

        float* d[2] = { buffer.getWritePointer (0),
                        chans > 1 ? buffer.getWritePointer (1) : nullptr };

        for (int i = 0; i < n; ++i)
        {
            const float a = amount.getNextValue();
            for (int c = 0; c < chans; ++c)
                d[c][i] += a * filters[c].processHigh (d[c][i]);
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
