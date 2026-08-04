// Four-band Linkwitz-Riley (24 dB/oct) crossover tree with allpass
// compensation, so the four bands sum back to a pure allpass of the input:
// flat magnitude by construction. See docs/ARCHITECTURE.md for the derivation.
//
//   lowHalf  = LP4(f2, x)          highHalf = HP4(f2, x)
//   band0    = AP2(f3, LP4(f1, lowHalf))
//   band1    = AP2(f3, HP4(f1, lowHalf))
//   band2    = AP2(f1, LP4(f3, highHalf))
//   band3    = AP2(f1, HP4(f3, highHalf))
//
// Cutoffs are smoothed and coefficients recomputed on a 32-sample control
// grid, which makes crossover automation click-free without per-sample tan().

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Core/ParameterIds.h"
#include "TptFilters.h"

namespace fourcolor
{
    class Crossover
    {
    public:
        Crossover() = default;

        static constexpr int controlInterval = 32;   // samples between coefficient updates
        static constexpr float minRatio = 1.30f;     // enforced spacing between adjacent cuts

        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset();

        //  Target frequencies; smoothed internally. Spacing is enforced here:
        //  f2 is authoritative, f1 is pushed down and f3 pushed up if needed.
        void setFrequencies (float f1, float f2, float f3) noexcept;

        //  Splits `input` into four band buffers. Each destination must have at
        //  least the input's channel count and length. Also writes the allpass
        //  reference (what the four bands sum to) into `allpassRef` if non-null.
        void process (const juce::AudioBuffer<float>& input,
                      juce::AudioBuffer<float>* bands,           // array of numBands buffers
                      juce::AudioBuffer<float>* allpassRef = nullptr) noexcept;

        //  The frequencies actually in use after spacing enforcement + smoothing.
        float getCurrentFrequency (int index) const noexcept { return currentHz[index]; }

    private:
        void updateCoefficients() noexcept;

        struct ChannelState
        {
            //  f2 split: shared first stage + one second stage per side.
            dsp::TptSvf mid1, midLow2, midHigh2;
            //  f1 split of the low half.
            dsp::TptSvf low1, lowLow2, lowHigh2;
            //  f3 split of the high half.
            dsp::TptSvf high1, highLow2, highHigh2;
            //  Compensation allpasses.
            dsp::TptSvf apLowSide;    // AP2 at f3, applied to bands 0 and 1 input
            dsp::TptSvf apHighSide;   // AP2 at f1, applied to bands 2 and 3 input
            //  Reference path: AP2(f1) AP2(f2) AP2(f3).
            dsp::TptSvf refAp1, refAp2, refAp3;

            void reset()
            {
                for (auto* f : { &mid1, &midLow2, &midHigh2, &low1, &lowLow2, &lowHigh2,
                                 &high1, &highLow2, &highHigh2,
                                 &apLowSide, &apHighSide, &refAp1, &refAp2, &refAp3 })
                    f->reset();
            }
        };

        double sampleRate = 44100.0;
        int numChannels = 2;

        float targetHz[3]  = { 120.0f, 700.0f, 4500.0f };
        float currentHz[3] = { 120.0f, 700.0f, 4500.0f };
        float smoothCoeff  = 0.0f;   // per-control-tick one-pole coefficient

        ChannelState channels[2];

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Crossover)
    };
}
