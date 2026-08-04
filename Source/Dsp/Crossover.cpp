#include "Crossover.h"

namespace fourcolor
{
    namespace
    {
        constexpr float butterworthK = 1.41421356f;   // 1/Q for a Butterworth section
    }

    void Crossover::prepare (double newSampleRate, int, int channelCount)
    {
        sampleRate  = newSampleRate;
        numChannels = juce::jlimit (1, 2, channelCount);

        //  ~15 ms smoothing time constant, applied once per control tick.
        const float tickRate = (float) sampleRate / (float) controlInterval;
        smoothCoeff = 1.0f - std::exp (-1.0f / (0.015f * tickRate));

        reset();
    }

    void Crossover::reset()
    {
        for (auto& ch : channels)
            ch.reset();

        for (int i = 0; i < 3; ++i)
            currentHz[i] = targetHz[i];

        updateCoefficients();
    }

    void Crossover::setFrequencies (float f1, float f2, float f3) noexcept
    {
        //  f2 is authoritative; push the outer cuts away if the user (or a
        //  preset) tries to cross them.
        f1 = juce::jmin (f1, f2 / minRatio);
        f3 = juce::jmax (f3, f2 * minRatio);

        targetHz[0] = f1;
        targetHz[1] = f2;
        targetHz[2] = f3;
    }

    void Crossover::updateCoefficients() noexcept
    {
        const float g1 = dsp::svfG (sampleRate, currentHz[0]);
        const float g2 = dsp::svfG (sampleRate, currentHz[1]);
        const float g3 = dsp::svfG (sampleRate, currentHz[2]);

        for (int c = 0; c < numChannels; ++c)
        {
            auto& ch = channels[c];

            ch.mid1.setCoefficients (g2, butterworthK);
            ch.midLow2.setCoefficients (g2, butterworthK);
            ch.midHigh2.setCoefficients (g2, butterworthK);

            ch.low1.setCoefficients (g1, butterworthK);
            ch.lowLow2.setCoefficients (g1, butterworthK);
            ch.lowHigh2.setCoefficients (g1, butterworthK);

            ch.high1.setCoefficients (g3, butterworthK);
            ch.highLow2.setCoefficients (g3, butterworthK);
            ch.highHigh2.setCoefficients (g3, butterworthK);

            ch.apLowSide.setCoefficients (g3, butterworthK);
            ch.apHighSide.setCoefficients (g1, butterworthK);

            ch.refAp1.setCoefficients (g1, butterworthK);
            ch.refAp2.setCoefficients (g2, butterworthK);
            ch.refAp3.setCoefficients (g3, butterworthK);
        }
    }

    void Crossover::process (const juce::AudioBuffer<float>& input,
                             juce::AudioBuffer<float>* bands,
                             juce::AudioBuffer<float>* allpassRef) noexcept
    {
        const int numSamples = input.getNumSamples();
        const int chans      = juce::jmin (input.getNumChannels(), numChannels);

        for (int start = 0; start < numSamples; start += controlInterval)
        {
            const int end = juce::jmin (start + controlInterval, numSamples);

            //  Smooth the cutoffs once per control tick.
            bool moved = false;
            for (int i = 0; i < 3; ++i)
            {
                const float delta = targetHz[i] - currentHz[i];
                if (std::abs (delta) > 1.0e-3f)
                {
                    currentHz[i] += smoothCoeff * delta;
                    moved = true;
                }
            }
            if (moved)
                updateCoefficients();

            for (int c = 0; c < chans; ++c)
            {
                auto& ch      = channels[c];
                auto* in      = input.getReadPointer (c);
                auto* b0      = bands[0].getWritePointer (c);
                auto* b1      = bands[1].getWritePointer (c);
                auto* b2      = bands[2].getWritePointer (c);
                auto* b3      = bands[3].getWritePointer (c);
                auto* ref     = allpassRef != nullptr ? allpassRef->getWritePointer (c) : nullptr;

                for (int i = start; i < end; ++i)
                {
                    const float x = in[i];

                    //  Split at f2. The first LR4 stage is shared: one SVF
                    //  yields both the LP2 and HP2 of the same input.
                    const auto m1  = ch.mid1.process (x);
                    float lowHalf  = ch.midLow2.processLow (m1.low);
                    float highHalf = ch.midHigh2.processHigh (m1.high);

                    //  Sibling allpass compensation (see ARCHITECTURE.md).
                    lowHalf  = ch.apLowSide.processAllpass (lowHalf);
                    highHalf = ch.apHighSide.processAllpass (highHalf);

                    //  Split the low half at f1.
                    const auto l1 = ch.low1.process (lowHalf);
                    b0[i] = ch.lowLow2.processLow (l1.low);
                    b1[i] = ch.lowHigh2.processHigh (l1.high);

                    //  Split the high half at f3.
                    const auto h1 = ch.high1.process (highHalf);
                    b2[i] = ch.highLow2.processLow (h1.low);
                    b3[i] = ch.highHigh2.processHigh (h1.high);

                    if (ref != nullptr)
                        ref[i] = ch.refAp3.processAllpass (
                                     ch.refAp2.processAllpass (
                                         ch.refAp1.processAllpass (x)));
                }
            }
        }
    }
}
