// One band's oversampled nonlinear section: four always-prepared colour
// engines, four pre-allocated oversamplers (1x/2x/4x/8x), a 15 ms equal-power
// crossfade on colour changes, and linear upsampling of the Behavior
// modulation signal to the oversampled rate.
//
// Everything is allocated in prepare(); quality and colour switches on the
// audio thread only swap indices and reset pre-existing state.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "ColorEngine.h"

namespace fourcolor
{
    class NonlinearStage
    {
    public:
        NonlinearStage() = default;

        void prepare (double baseSampleRate, int maxBlockSize, int numChannels);
        void reset();

        //  Audio-thread safe: switches between pre-allocated oversamplers at a
        //  block boundary. Returns true if the factor actually changed (the
        //  caller must then re-align delays and re-report latency).
        bool setQuality (Quality q) noexcept;

        //  Audio-thread safe: starts the colour crossfade if the type changed.
        void setColor (ColorType type) noexcept;

        void setDrive (float drivePercent) noexcept;

        //  In-place. `behaviorMod`, if given, is a base-rate per-sample linear
        //  gain factor (around 1.0) applied to the engine pre-gain.
        void process (juce::AudioBuffer<float>& buffer,
                      const float* behaviorMod[2] = nullptr) noexcept;

        //  Wet-path latency at the base rate for the CURRENT quality (fractional).
        float getLatencySamples() const noexcept;

        //  Worst-case latency across all qualities, for sizing alignment delays.
        float getMaxLatencySamples() const noexcept { return maxLatency; }

        //  Applied internally by process(); exposed for the tests.
        float getCompensationGain() const noexcept
        {
            return engines[(size_t) activeColor]->getCompensationGain();
        }

        ColorType getColor() const noexcept { return activeColor; }

    private:
        double baseRate = 48000.0;
        int maxBlock = 0;
        int channels = 2;
        float maxLatency = 0.0f;

        std::unique_ptr<ColorEngine> engines[4];         // per ColorType, at 8x readiness
        std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[4];   // per Quality
        int activeQuality = (int) Quality::high;

        ColorType activeColor = ColorType::warm;
        ColorType fadingFrom  = ColorType::warm;
        int fadeSamplesLeft = 0;      // at base rate
        int fadeLengthSamples = 1;

        juce::AudioBuffer<float> fadeScratch;    // oversampled copy for the outgoing colour
        juce::AudioBuffer<float> modScratch;     // oversampled behavior modulation
        float drivePercent = 25.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NonlinearStage)
    };
}
