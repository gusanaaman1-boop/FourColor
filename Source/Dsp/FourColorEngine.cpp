#include "FourColorEngine.h"

namespace fourcolor
{
    void FourColorEngine::prepare (double newSampleRate, int maxBlockSize, int channels)
    {
        sampleRate  = newSampleRate;
        maxBlock    = maxBlockSize;
        numChannels = juce::jlimit (1, 2, channels);

        dryBuffer.setSize (numChannels, maxBlockSize);

        const double fastSmooth = 0.02;   // 20 ms for gains
        inputGain .reset (sampleRate, fastSmooth);
        outputGain.reset (sampleRate, fastSmooth);
        mixSmoothed.reset (sampleRate, fastSmooth);
        bypassFade .reset (sampleRate, 0.01);

        latencySamples = 0;   // Phase 1: no oversampling yet

        reset();
    }

    void FourColorEngine::reset()
    {
        dryBuffer.clear();
        inputGain .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.inputDb));
        outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outputDb));
        mixSmoothed.setCurrentAndTargetValue (params.mixPercent * 0.01f);
        bypassFade .setCurrentAndTargetValue (params.bypassed ? 1.0f : 0.0f);
    }

    void FourColorEngine::setParameters (const EngineParameters& p) noexcept
    {
        params = p;
        inputGain .setTargetValue (juce::Decibels::decibelsToGain (p.inputDb));
        outputGain.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb));
        mixSmoothed.setTargetValue (p.mixPercent * 0.01f);
        bypassFade .setTargetValue (p.bypassed ? 1.0f : 0.0f);
    }

    void FourColorEngine::process (juce::AudioBuffer<float>& buffer) noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        const int numSamples = buffer.getNumSamples();
        const int channels   = juce::jmin (buffer.getNumChannels(), numChannels);

        if (numSamples == 0)
            return;

        // Keep the untouched input for dry/wet and bypass. With zero latency the
        // aligned dry signal is the input itself.
        for (int ch = 0; ch < channels; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float gIn  = inputGain.getNextValue();
            const float gOut = outputGain.getNextValue();
            const float mix  = mixSmoothed.getNextValue();
            const float byp  = bypassFade.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                const float dry = dryBuffer.getSample (ch, i);

                // Phase 1: the "wet" chain is input trim -> unity -> output trim.
                float wet = dry * gIn;
                wet *= gOut;

                const float mixed = dry + mix * (wet - dry);
                buffer.setSample (ch, i, mixed + byp * (dry - mixed));
            }
        }

        applySafety (buffer);
    }

    void FourColorEngine::applySafety (juce::AudioBuffer<float>& buffer) noexcept
    {
        // Last line of defence: scrub non-finite samples and softly limit
        // anything absurd so a DSP fault can never blast the monitors.
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float x = data[i];

                if (! std::isfinite (x))
                    x = 0.0f;
                else if (std::abs (x) > 4.0f)
                    x = (x > 0.0f ? 4.0f : -4.0f);

                data[i] = x;
            }
        }
    }
}
