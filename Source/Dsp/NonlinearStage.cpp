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

        //  Engines are prepared for the highest oversampled rate they might see;
        //  time constants are recomputed on every quality switch below, so
        //  prepare each engine for the CURRENT factor at switch time instead.
        for (int e = 0; e < 4; ++e)
        {
            engines[e] = createColorEngine ((ColorType) e);
            engines[e]->prepare (baseRate * oversamplers[activeQuality]->getOversamplingFactor(),
                                 channels);
        }

        //  Both scratch buffers live in the oversampled domain: 8x worst case.
        fadeScratch.setSize (channels, maxBlock * 8);
        modScratch.setSize (channels, maxBlock * 8);

        fadeLengthSamples = juce::jmax (1, (int) (0.015 * baseRate));
        fadeSamplesLeft = 0;

        setDrive (drivePercent);
        reset();
    }

    void NonlinearStage::reset()
    {
        for (auto& os : oversamplers)
            if (os != nullptr)
                os->reset();

        for (auto& e : engines)
            if (e != nullptr)
                e->reset();

        fadeSamplesLeft = 0;
    }

    bool NonlinearStage::setQuality (Quality q) noexcept
    {
        const int index = juce::jlimit (0, 3, (int) q);
        if (index == activeQuality)
            return false;

        activeQuality = index;
        oversamplers[activeQuality]->reset();

        //  Re-derive engine time constants for the new engine-side rate.
        //  ColorEngine::prepare only computes coefficients - no allocation.
        const double engineRate = baseRate * oversamplers[activeQuality]->getOversamplingFactor();
        for (auto& e : engines)
            e->prepare (engineRate, channels);

        return true;
    }

    void NonlinearStage::setColor (ColorType type) noexcept
    {
        if (type == activeColor)
            return;

        fadingFrom = activeColor;
        activeColor = type;
        fadeSamplesLeft = fadeLengthSamples;
        engines[(size_t) activeColor]->setDrive (drivePercent);
    }

    void NonlinearStage::setDrive (float newDrivePercent) noexcept
    {
        drivePercent = newDrivePercent;
        for (auto& e : engines)
            if (e != nullptr)
                e->setDrive (drivePercent);
    }

    float NonlinearStage::getLatencySamples() const noexcept
    {
        return (float) oversamplers[activeQuality]->getLatencyInSamples();
    }

    void NonlinearStage::process (juce::AudioBuffer<float>& buffer,
                                  const float* behaviorMod[2]) noexcept
    {
        const int n = buffer.getNumSamples();
        if (n == 0)
            return;

        auto& os = *oversamplers[activeQuality];
        const int factor = (int) os.getOversamplingFactor();

        //  Upsample ONCE; engines and the colour crossfade all run in the
        //  oversampled domain, so both fade legs share the same latency and
        //  the same anti-aliasing.
        juce::dsp::AudioBlock<float> baseBlock (buffer.getArrayOfWritePointers(),
                                                (size_t) buffer.getNumChannels(), (size_t) n);
        auto osBlock = os.processSamplesUp (baseBlock);
        const int osSamples = (int) osBlock.getNumSamples();
        const int chans = (int) osBlock.getNumChannels();

        //  Upsample the (smooth, low-frequency) Behavior modulation linearly.
        const float* mod[2] = { nullptr, nullptr };
        if (behaviorMod != nullptr)
        {
            for (int c = 0; c < chans; ++c)
            {
                if (behaviorMod[c] == nullptr)
                    continue;

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
        }

        auto& incoming = *engines[(size_t) activeColor];
        const float compIn = incoming.getCompensationGain();

        if (fadeSamplesLeft <= 0)
        {
            for (int c = 0; c < chans; ++c)
            {
                auto* d = osBlock.getChannelPointer ((size_t) c);
                incoming.processBlock (d, osSamples, c, mod[c]);

                //  Static make-up so Drive changes colour more than loudness.
                juce::FloatVectorOperations::multiply (d, compIn, osSamples);
            }
        }
        else
        {
            //  Equal-power crossfade in the oversampled domain: outgoing engine
            //  on a copy, incoming on the block, blended per sample.
            auto& outgoing = *engines[(size_t) fadingFrom];
            const float compOut = outgoing.getCompensationGain();
            const int fadeLenOs = fadeLengthSamples * factor;
            const int leftAtStart = fadeSamplesLeft * factor;

            for (int c = 0; c < chans; ++c)
            {
                auto* d = osBlock.getChannelPointer ((size_t) c);
                auto* s = fadeScratch.getWritePointer (c);
                juce::FloatVectorOperations::copy (s, d, osSamples);

                incoming.processBlock (d, osSamples, c, mod[c]);
                outgoing.processBlock (s, osSamples, c, mod[c]);

                int left = leftAtStart;
                for (int i = 0; i < osSamples; ++i)
                {
                    if (left > 0)
                    {
                        const float t = 1.0f - (float) left / (float) fadeLenOs;   // 0 -> 1
                        const float a = std::sin (t * juce::MathConstants<float>::halfPi);
                        const float b = std::cos (t * juce::MathConstants<float>::halfPi);
                        d[i] = a * d[i] * compIn + b * s[i] * compOut;
                        --left;
                    }
                    else
                    {
                        d[i] *= compIn;
                    }
                }
            }

            fadeSamplesLeft = juce::jmax (0, fadeSamplesLeft - n);
        }

        os.processSamplesDown (baseBlock);
    }
}
