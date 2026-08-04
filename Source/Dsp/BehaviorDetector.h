// The BODY <-> ATTACK detector. Two envelope followers on the same stereo-
// linked signal - one fast, one slow - and a bounded transient measure derived
// from their difference. The measure modulates the colour engine's pre-gain:
//
//   ATTACK (+): transients are driven HARDER   (hit gets bite, tail stays calm)
//   BODY   (-): transients are driven SOFTER   (hit passes clean, body saturates)
//
// Time constants scale per band: a "fast" 1 ms follower on a 40 Hz band would
// just track the waveform, so the low bands use slower clocks than the top.
// Modulation depth is bounded to +/-6 dB and smoothed, which is what keeps
// this from becoming a pumping compressor.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Core/ParameterIds.h"

namespace fourcolor
{
    class BehaviorDetector
    {
    public:
        BehaviorDetector() = default;

        void prepare (double sampleRate, int bandIndex);
        void reset();

        //  behavior in [-1, 1]; 0 disables (writeModulation then writes 1.0).
        void setBehavior (float behavior) noexcept { amount = juce::jlimit (-1.0f, 1.0f, behavior); }

        //  Reads the (pre-processing) band buffer, writes a per-sample linear
        //  pre-gain factor into `modOut` (length n). Stereo-linked: one curve
        //  for both channels.
        void writeModulation (const juce::AudioBuffer<float>& bandInput,
                              float* modOut, int n) noexcept;

        bool isActive() const noexcept { return std::abs (amount) > 1.0e-3f; }

    private:
        float amount = 0.0f;

        //  One-pole coefficients (attack/release pairs) for the two followers
        //  and the modulation smoother.
        float fastAtk = 0.0f, fastRel = 0.0f;
        float slowAtk = 0.0f, slowRel = 0.0f;
        float modSmooth = 0.0f;

        float fastEnv = 0.0f, slowEnv = 0.0f, modState = 1.0f;

        //  +/-9 dB at full deflection: +/-6 dB was measured to move the kick's
        //  attack-crunch spread only 1.3 dB, too subtle for the product's
        //  headline control. Pumping stays bounded (asserted at < 1 dB on
        //  sustained material).
        static constexpr float maxModDb = 9.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BehaviorDetector)
    };
}
