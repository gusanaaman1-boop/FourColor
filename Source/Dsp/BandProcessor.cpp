#include "BandProcessor.h"

namespace fourcolor
{
    void BandProcessor::prepare (double sampleRate, int maxBlockSize, int numChannels, int index)
    {
        rate = sampleRate;
        maxBlock = maxBlockSize;
        channels = juce::jlimit (1, 2, numChannels);
        bandIndex = index;

        nonlinear.prepare (sampleRate, maxBlockSize, channels);
        tone.prepare (sampleRate, channels);

        cleanBuffer.setSize (channels, maxBlockSize);

        //  Sized for the worst oversampler latency plus interpolation headroom.
        cleanDelay.setMaximumDelayInSamples ((int) std::ceil (nonlinear.getMaxLatencySamples()) + 8);
        cleanDelay.prepare ({ sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) channels });

        const double smoothTime = 0.02;
        levelGain.reset (sampleRate, smoothTime);
        mixSmoothed.reset (sampleRate, smoothTime);
        muteFade.reset (sampleRate, 0.01);
        bypassFade.reset (sampleRate, 0.01);
        soloFade.reset (sampleRate, 0.01);
        driveSmoothed.reset (sampleRate, 0.05);

        updateCleanDelay();
        reset();
    }

    void BandProcessor::reset()
    {
        nonlinear.reset();
        tone.reset();
        cleanDelay.reset();
        cleanBuffer.clear();

        levelGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (settings.levelDb));
        mixSmoothed.setCurrentAndTargetValue (settings.bandMix);
        muteFade.setCurrentAndTargetValue (settings.mute ? 0.0f : 1.0f);
        bypassFade.setCurrentAndTargetValue (settings.bypass ? 1.0f : 0.0f);
        soloFade.setCurrentAndTargetValue (settings.soloGain);
        driveSmoothed.setCurrentAndTargetValue (settings.drivePercent);
    }

    void BandProcessor::updateCleanDelay() noexcept
    {
        cleanDelay.setDelay (juce::jmax (0.0f, nonlinear.getLatencySamples()));
    }

    void BandProcessor::setSettings (const Settings& s) noexcept
    {
        settings = s;

        nonlinear.setColor (s.color);
        driveSmoothed.setTargetValue (s.drivePercent);
        tone.setTone (s.tone, s.centreHz);

        levelGain.setTargetValue (juce::Decibels::decibelsToGain (s.levelDb));
        mixSmoothed.setTargetValue (s.bandMix);
        muteFade.setTargetValue (s.mute ? 0.0f : 1.0f);
        bypassFade.setTargetValue (s.bypass ? 1.0f : 0.0f);
        soloFade.setTargetValue (s.soloGain);
    }

    bool BandProcessor::setQuality (Quality q) noexcept
    {
        const bool changed = nonlinear.setQuality (q);
        if (changed)
        {
            cleanDelay.reset();
            updateCleanDelay();
        }
        return changed;
    }

    void BandProcessor::process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples();
        const int chans = juce::jmin (buffer.getNumChannels(), channels);
        if (n == 0)
            return;

        //  Clean reference, delayed to align with the oversampler's latency.
        for (int c = 0; c < chans; ++c)
        {
            auto* src = buffer.getReadPointer (c);
            auto* dst = cleanBuffer.getWritePointer (c);

            for (int i = 0; i < n; ++i)
            {
                cleanDelay.pushSample (c, src[i]);
                dst[i] = cleanDelay.popSample (c);
            }
        }

        //  Drive is smoothed at block granularity: the engines' curves are
        //  continuous in drive, so 32-512 sample steps of a smoothed value
        //  stay click-free (asserted by the automation test).
        nonlinear.setDrive (driveSmoothed.skip (n));

        //  Wet path: pre-tone -> oversampled colour -> post-tone.
        tone.processPre (buffer);
        nonlinear.process (buffer);
        tone.processPost (buffer);

        //  Mix / level / mute / bypass / solo, all with per-sample fades.
        //  Samples outer, channels inner, so each SmoothedValue advances
        //  exactly once per sample and both channels get identical gains.
        float* wet[2]   = { buffer.getWritePointer (0),
                            chans > 1 ? buffer.getWritePointer (1) : nullptr };
        const float* clean[2] = { cleanBuffer.getReadPointer (0),
                                  chans > 1 ? cleanBuffer.getReadPointer (1) : nullptr };

        for (int i = 0; i < n; ++i)
        {
            const float mix  = mixSmoothed.getNextValue();
            const float lvl  = levelGain.getNextValue();
            const float mute = muteFade.getNextValue();
            const float byp  = bypassFade.getNextValue();
            const float solo = soloFade.getNextValue();

            for (int c = 0; c < chans; ++c)
            {
                const float mixed   = clean[c][i] + mix * (wet[c][i] - clean[c][i]);
                const float leveled = mixed * lvl;
                //  Bypass returns the UNleveled clean band, so four bypassed
                //  bands reconstruct the input exactly.
                const float out = leveled + byp * (clean[c][i] - leveled);
                wet[c][i] = out * mute * solo;
            }
        }
    }
}
