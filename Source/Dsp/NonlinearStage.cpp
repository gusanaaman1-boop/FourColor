#include "NonlinearStage.h"

namespace fourcolor
{
    void NonlinearStage::prepare (double newBaseRate, int maxBlockSize, int numChannels)
    {
        baseRate = newBaseRate;
        maxBlock = maxBlockSize;
        channels = juce::jlimit (1, 2, numChannels);

        //  All four oversamplers exist up front so a Quality change on the
        //  audio thread never allocates.
        maxLatency = 0.0f;
        for (int q = 0; q < 4; ++q)
        {
            //  Quality index q doubles as the oversampling log2: 0..3 -> 1x..8x.
            oversamplers[q] = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) channels, (size_t) q,
                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                true /* isMaxQuality */);
            oversamplers[q]->initProcessing ((size_t) maxBlock);
            maxLatency = juce::jmax (maxLatency, (float) oversamplers[q]->getLatencyInSamples());
        }

        //  Two engine banks. Both are created here; a quality switch only ever
        //  recomputes coefficients on one of them.
        for (int bank = 0; bank < 2; ++bank)
            for (int e = 0; e < 4; ++e)
                engines[bank][e] = createColorEngine ((ColorType) e);

        //  The common target: an integer, which is what makes every pad an
        //  exact whole number of OVERSAMPLED samples (see the header).
        reportedLatency = std::ceil (maxLatency);

        //  Worst pad in oversampled samples is 1x's, which is reportedLatency.
        juce::dsp::ProcessSpec spec { baseRate, (juce::uint32) (maxBlock * 8),
                                      (juce::uint32) channels };
        for (auto& pad : padDelay)
        {
            pad.setMaximumDelayInSamples ((int) reportedLatency + 4);
            pad.prepare (spec);
        }

        //  Both scratch buffers live in the oversampled domain: 8x worst case.
        fadeScratch.setSize (channels, maxBlock * 8);
        modScratch.setSize (channels, maxBlock * 8);
        qualityScratch.setSize (channels, maxBlock);

        fadeLengthSamples = juce::jmax (1, (int) (0.015 * baseRate));

        //  The quality fade is deliberately twice the colour fade. Crossfading
        //  1x against 8x means crossfading two genuinely different signals -
        //  one aliases, one does not - so the slower the blend, the smaller the
        //  instantaneous difference. A Quality change is a structural switch,
        //  not a musical gesture; 30 ms is imperceptible.
        qualityFadeLength = juce::jmax (1, (int) (0.030 * baseRate));

        //  The incoming path needs its oversampler, its pad and its engines to
        //  fill before it is worth listening to. maxLatency samples covers all
        //  three, because pad + own latency == maxLatency by construction.
        qualityPreRoll = (int) std::ceil (maxLatency) + 1;

        activeBank = 0;
        qualityFadeLeft = 0;
        pendingQuality = -1;
        fadeSamplesLeft = 0;

        armBank (activeBank, activeQuality);
        setDrive (drivePercent);
        reset();
    }

    //  Points a bank's engines at a quality's oversampled rate and sets that
    //  bank's pad so the path totals maxLatency. Coefficient work only - no
    //  allocation, safe on the audio thread.
    void NonlinearStage::armBank (int bankIndex, int qualityIndex) noexcept
    {
        const double engineRate = baseRate
                                * (double) oversamplers[qualityIndex]->getOversamplingFactor();

        auto ctx = bandContext;
        ctx.oversampledRate = engineRate;

        for (auto& e : engines[bankIndex])
        {
            e->prepare (engineRate, channels);
            e->setDrive (drivePercent);
            e->setContext (ctx);
        }

        //  In the oversampled domain the pad is exact: the oversampler's own
        //  latency times its factor is always a whole number of samples, and
        //  so is reportedLatency times its factor, because both are integers.
        const int factor = (int) oversamplers[qualityIndex]->getOversamplingFactor();
        const auto ownOs = (int) std::llround (oversamplers[qualityIndex]->getLatencyInSamples()
                                                   * (double) factor);
        padOsSamples[bankIndex] = juce::jmax (0, (int) reportedLatency * factor - ownOs);
        padDelay[bankIndex].setDelay ((float) padOsSamples[bankIndex]);
    }

    void NonlinearStage::reset()
    {
        for (auto& os : oversamplers)
            if (os != nullptr)
                os->reset();

        for (auto& bank : engines)
            for (auto& e : bank)
                if (e != nullptr)
                    e->reset();

        for (auto& pad : padDelay)
            pad.reset();

        fadeSamplesLeft = 0;
        qualityFadeLeft = 0;
        pendingQuality = -1;
    }

    bool NonlinearStage::setQuality (Quality q) noexcept
    {
        const int index = juce::jlimit (0, 3, (int) q);

        if (qualityFadeLeft > 0)
        {
            //  Already crossfading. Remember the request instead of tearing
            //  down a fade that is halfway through.
            pendingQuality = (index == activeQuality) ? -1 : index;
            return false;
        }

        if (index == activeQuality)
            return false;

        outgoingQuality = activeQuality;
        activeQuality = index;

        //  The outgoing bank keeps its warm state; the incoming bank is armed
        //  for the new rate and starts from a clean slate, which is what the
        //  pre-roll and the fade are for.
        activeBank = 1 - activeBank;
        armBank (activeBank, activeQuality);
        padDelay[activeBank].reset();
        oversamplers[activeQuality]->reset();

        qualityFadeTotal = qualityPreRoll + qualityFadeLength;
        qualityFadeLeft = qualityFadeTotal;
        return true;
    }

    void NonlinearStage::setColor (ColorType type) noexcept
    {
        if (type == activeColor)
            return;

        fadingFrom = activeColor;
        activeColor = type;
        fadeSamplesLeft = fadeLengthSamples;

        for (auto& bank : engines)
            bank[(size_t) activeColor]->setDrive (drivePercent);
    }

    void NonlinearStage::setContext (const ColorContext& c) noexcept
    {
        bandContext = c;

        //  Both banks: during a quality crossfade the outgoing one is still
        //  producing audio, and a band edge that reached only one of them would
        //  make the two paths disagree in the middle of the fade.
        for (int bank = 0; bank < 2; ++bank)
        {
            const int q = bank == activeBank ? activeQuality : outgoingQuality;

            if (oversamplers[q] == nullptr)
                continue;

            auto ctx = c;
            ctx.oversampledRate = baseRate * (double) oversamplers[q]->getOversamplingFactor();

            for (auto& e : engines[bank])
                e->setContext (ctx);
        }
    }

    void NonlinearStage::setDrive (float newDrivePercent) noexcept
    {
        drivePercent = newDrivePercent;
        for (auto& bank : engines)
            for (auto& e : bank)
                if (e != nullptr)
                    e->setDrive (drivePercent);
    }

    void NonlinearStage::processPath (int qualityIndex, int bankIndex,
                                      juce::AudioBuffer<float>& buffer, int n,
                                      const float* const* mod, int colourFadeLeft) noexcept
    {
        auto& os = *oversamplers[qualityIndex];
        const int factor = (int) os.getOversamplingFactor();

        juce::dsp::AudioBlock<float> baseBlock (buffer.getArrayOfWritePointers(),
                                                (size_t) buffer.getNumChannels(), (size_t) n);
        auto osBlock = os.processSamplesUp (baseBlock);
        const int osSamples = (int) osBlock.getNumSamples();
        const int chans = (int) osBlock.getNumChannels();

        auto& incoming = *engines[bankIndex][(size_t) activeColor];

        if (colourFadeLeft <= 0)
        {
            for (int c = 0; c < chans; ++c)
                incoming.processBlock (osBlock.getChannelPointer ((size_t) c),
                                       osSamples, c, mod[c]);
        }
        else
        {
            //  Equal-power crossfade in the oversampled domain: outgoing engine
            //  on a copy, incoming on the block, blended per sample.
            auto& outgoing = *engines[bankIndex][(size_t) fadingFrom];
            const int fadeLenOs = fadeLengthSamples * factor;
            const int leftAtStart = colourFadeLeft * factor;

            for (int c = 0; c < chans; ++c)
            {
                auto* d = osBlock.getChannelPointer ((size_t) c);
                auto* s = fadeScratch.getWritePointer (c);
                juce::FloatVectorOperations::copy (s, d, osSamples);

                incoming.processBlock (d, osSamples, c, mod[c]);
                outgoing.processBlock (s, osSamples, c, mod[c]);

                int left = leftAtStart;
                for (int i = 0; i < osSamples && left > 0; ++i)
                {
                    const float t = 1.0f - (float) left / (float) fadeLenOs;   // 0 -> 1
                    const float a = std::sin (t * juce::MathConstants<float>::halfPi);
                    const float b = std::cos (t * juce::MathConstants<float>::halfPi);
                    d[i] = a * d[i] + b * s[i];
                    --left;
                }
            }
        }

        //  Pad this path up to the common latency, BEFORE downsampling: here
        //  the pad is a whole number of samples for every factor, so it is a
        //  pure delay with no interpolation and no filtering.
        if (padOsSamples[bankIndex] > 0)
        {
            auto& pad = padDelay[bankIndex];
            for (int c = 0; c < chans; ++c)
            {
                auto* d = osBlock.getChannelPointer ((size_t) c);
                for (int i = 0; i < osSamples; ++i)
                {
                    pad.pushSample (c, d[i]);
                    d[i] = pad.popSample (c);
                }
            }
        }

        os.processSamplesDown (baseBlock);
    }

    void NonlinearStage::process (juce::AudioBuffer<float>& buffer,
                                  const float* behaviorMod[2]) noexcept
    {
        const int n = buffer.getNumSamples();
        if (n == 0)
            return;

        const int chans = juce::jmin (channels, buffer.getNumChannels());

        //  Upsample the (smooth, low-frequency) Behavior modulation linearly,
        //  at the ACTIVE factor. During a quality fade the outgoing path uses
        //  the same curve resampled to its own factor.
        const float* mod[2] = { nullptr, nullptr };
        auto buildMod = [&] (int factor)
        {
            if (behaviorMod == nullptr)
                return;

            const int osSamples = n * factor;
            for (int c = 0; c < chans; ++c)
            {
                if (behaviorMod[c] == nullptr)
                {
                    mod[c] = nullptr;
                    continue;
                }

                auto* m = modScratch.getWritePointer (c);
                const float* src = behaviorMod[c];

                for (int i = 0; i < osSamples; ++i)
                {
                    const float pos = (float) i / (float) factor;
                    const int i0 = juce::jmin ((int) pos, n - 1);
                    const int i1 = juce::jmin (i0 + 1, n - 1);
                    const float frac = pos - (float) i0;
                    m[i] = src[i0] + frac * (src[i1] - src[i0]);
                }
                mod[c] = m;
            }
        };

        const int colourFadeAtStart = fadeSamplesLeft;

        if (qualityFadeLeft <= 0)
        {
            buildMod ((int) oversamplers[activeQuality]->getOversamplingFactor());
            processPath (activeQuality, activeBank, buffer, n, mod, colourFadeAtStart);
        }
        else
        {
            const int outgoingBank = 1 - activeBank;

            //  Outgoing path on a copy of the input. qualityScratch was sized
            //  in prepare(); nothing is resized on the audio thread.
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                juce::FloatVectorOperations::copy (qualityScratch.getWritePointer (c),
                                                   buffer.getReadPointer (c), n);

            buildMod ((int) oversamplers[outgoingQuality]->getOversamplingFactor());
            processPath (outgoingQuality, outgoingBank, qualityScratch, n, mod, colourFadeAtStart);

            //  ...the incoming path on the block itself.
            buildMod ((int) oversamplers[activeQuality]->getOversamplingFactor());
            processPath (activeQuality, activeBank, buffer, n, mod, colourFadeAtStart);

            //  Equal-power blend, after a pre-roll that lets the incoming path
            //  fill its oversampler, pad and engine state at zero weight.
            for (int c = 0; c < buffer.getNumChannels(); ++c)
            {
                auto* d = buffer.getWritePointer (c);
                const auto* s = qualityScratch.getReadPointer (c);

                int left = qualityFadeLeft;
                for (int i = 0; i < n; ++i)
                {
                    float w = 0.0f;
                    if (left <= 0)
                    {
                        w = 1.0f;
                    }
                    else
                    {
                        const int elapsed = qualityFadeTotal - left;
                        if (elapsed >= qualityPreRoll)
                        {
                            const float t = (float) (elapsed - qualityPreRoll)
                                          / (float) qualityFadeLength;
                            w = std::sin (juce::jlimit (0.0f, 1.0f, t)
                                              * juce::MathConstants<float>::halfPi);
                        }
                        --left;
                    }

                    const float wOut = std::sqrt (juce::jmax (0.0f, 1.0f - w * w));
                    d[i] = w * d[i] + wOut * s[i];
                }
            }

            qualityFadeLeft = juce::jmax (0, qualityFadeLeft - n);

            if (qualityFadeLeft == 0 && pendingQuality >= 0)
            {
                const int next = pendingQuality;
                pendingQuality = -1;
                setQuality ((Quality) next);
            }
        }

        if (fadeSamplesLeft > 0)
            fadeSamplesLeft = juce::jmax (0, fadeSamplesLeft - n);
    }
}
