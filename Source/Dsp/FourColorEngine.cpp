#include "FourColorEngine.h"

namespace fourcolor
{
    void FourColorEngine::prepare (double newSampleRate, int maxBlockSize, int channels)
    {
        sampleRate  = newSampleRate;
        maxBlock    = maxBlockSize;
        numChannels = juce::jlimit (1, 2, channels);

        crossover.prepare (sampleRate, maxBlockSize, numChannels);

        for (int b = 0; b < numBands; ++b)
        {
            bands[b].prepare (sampleRate, maxBlockSize, numChannels, b);
            bandBuffers[b].setSize (numChannels, maxBlockSize);
        }

        dryBuffer.setSize (numChannels, maxBlockSize);

        const int maxDelay = (int) std::ceil (bands[0].getNonlinearStage().getMaxLatencySamples()) + 8;
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize,
                                            (juce::uint32) numChannels };
        dryDelay.setMaximumDelayInSamples (maxDelay);
        dryDelay.prepare (spec);
        wetAlign.setMaximumDelayInSamples (8);
        wetAlign.prepare (spec);

        const double fastSmooth = 0.02;
        inputGain .reset (sampleRate, fastSmooth);
        outputGain.reset (sampleRate, fastSmooth);
        mixSmoothed.reset (sampleRate, fastSmooth);
        bypassFade .reset (sampleRate, 0.01);

        updateLatency();
        reset();
    }

    void FourColorEngine::updateLatency() noexcept
    {
        //  All bands share one oversampling factor, hence one latency.
        const float osLatency = bands[0].getLatencySamples();
        latencySamples = (int) std::ceil (osLatency);
        wetAlignDelay  = (float) latencySamples - osLatency;

        wetAlign.setDelay (wetAlignDelay);
        dryDelay.setDelay ((float) latencySamples);
    }

    void FourColorEngine::reset()
    {
        crossover.reset();
        for (auto& b : bands)
            b.reset();
        for (auto& bb : bandBuffers)
            bb.clear();

        dryBuffer.clear();
        dryDelay.reset();
        wetAlign.reset();

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

        crossover.setFrequencies (p.xoverHz[0], p.xoverHz[1], p.xoverHz[2]);

        //  Solo logic needs all four bands, so it is resolved here.
        bool anySolo = false;
        for (const auto& b : p.bands)
            anySolo = anySolo || b.solo;

        //  Band centres for the Tone shelves, from the current crossover cuts.
        const float f1 = p.xoverHz[0], f2 = p.xoverHz[1], f3 = p.xoverHz[2];
        const float centres[numBands] = {
            std::sqrt (20.0f * f1), std::sqrt (f1 * f2),
            std::sqrt (f2 * f3),    std::sqrt (f3 * 16000.0f)
        };

        bool qualityChanged = false;
        for (int b = 0; b < numBands; ++b)
        {
            qualityChanged = bands[b].setQuality (p.quality) || qualityChanged;

            BandProcessor::Settings s;
            s.color        = p.bands[b].color;
            s.drivePercent = p.bands[b].drive;
            s.behavior     = p.bands[b].behavior * 0.01f;
            s.tone         = p.bands[b].tone * 0.01f;
            s.spacePercent = p.bands[b].space;
            s.bandMix      = p.bands[b].bandMix * 0.01f;
            s.levelDb      = p.bands[b].levelDb;
            s.mute         = p.bands[b].mute;
            s.bypass       = p.bands[b].bypass;
            s.soloGain     = anySolo ? (p.bands[b].solo ? 1.0f : 0.0f) : 1.0f;
            s.centreHz     = centres[b];
            bands[b].setSettings (s);
        }

        if (qualityChanged)
        {
            //  New oversampler latency: realign both compensation delays. The
            //  host is told through getLatencySamples() by the processor.
            dryDelay.reset();
            wetAlign.reset();
            updateLatency();
        }
    }

    void FourColorEngine::process (juce::AudioBuffer<float>& buffer) noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        const int n = buffer.getNumSamples();
        const int chans = juce::jmin (buffer.getNumChannels(), numChannels);
        if (n == 0)
            return;

        //  Input trim feeds everything, including the dry tap.
        for (int i = 0; i < n; ++i)
        {
            const float g = inputGain.getNextValue();
            for (int c = 0; c < chans; ++c)
                buffer.getWritePointer (c)[i] *= g;
        }

        //  Dry tap, delayed by the reported (integer) latency.
        for (int c = 0; c < chans; ++c)
        {
            auto* src = buffer.getReadPointer (c);
            auto* dst = dryBuffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                dryDelay.pushSample (c, src[i]);
                dst[i] = dryDelay.popSample (c);
            }
        }

        //  Split, process each band, recombine.
        crossover.process (buffer, bandBuffers);

        for (int b = 0; b < numBands; ++b)
        {
            //  The band buffers were sized in prepare; give each processor a
            //  view of the current block length.
            juce::AudioBuffer<float> view (bandBuffers[b].getArrayOfWritePointers(), chans, n);
            bands[b].process (view);
        }

        buffer.clear();
        for (int b = 0; b < numBands; ++b)
            for (int c = 0; c < chans; ++c)
                buffer.addFrom (c, 0, bandBuffers[b], c, 0, n);

        //  Fractional wet alignment: wet total latency becomes exactly the
        //  integer reported to the host.
        if (wetAlignDelay > 1.0e-4f)
        {
            for (int c = 0; c < chans; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    wetAlign.pushSample (c, d[i]);
                    d[i] = wetAlign.popSample (c);
                }
            }
        }

        //  Latency-compensated dry/wet, output trim, global bypass.
        for (int i = 0; i < n; ++i)
        {
            const float mix  = mixSmoothed.getNextValue();
            const float gOut = outputGain.getNextValue();
            const float byp  = bypassFade.getNextValue();

            for (int c = 0; c < chans; ++c)
            {
                const float dry = dryBuffer.getSample (c, i);
                const float wet = buffer.getSample (c, i);

                const float mixed = (dry + mix * (wet - dry)) * gOut;
                buffer.setSample (c, i, mixed + byp * (dry - mixed));
            }
        }

        applySafety (buffer);
    }

    void FourColorEngine::applySafety (juce::AudioBuffer<float>& buffer) noexcept
    {
        //  Last line of defence: scrub non-finite samples and softly limit
        //  anything absurd so a DSP fault can never blast the monitors.
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
