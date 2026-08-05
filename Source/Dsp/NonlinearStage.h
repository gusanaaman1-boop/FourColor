// One band's oversampled nonlinear section: colour engines in two banks, four
// pre-allocated oversamplers (1x/2x/4x/8x), a 15 ms equal-power crossfade on
// colour changes, and linear upsampling of the Behavior modulation signal to
// the oversampled rate.
//
// LATENCY CONTRACT: every quality reports the SAME latency, ceil(worst case) =
// 65 samples at any rate JUCE's equiripple oversampler is used at here. The pad
// is applied INSIDE the oversampled domain, where it is an exact integer for
// every factor (65x1-0=65, 65x2-98=32, 65x4-238=22, 65x8-514=6), so no
// interpolating delay ever touches the audio. A fractional base-rate pad was
// measured to smear transients enough to cost BODY/ATTACK 0.6 dB of separation.
// 1x pays 65 samples it does not need; that is the price of a Quality control
// that is safe to touch during playback.
//
// QUALITY SWITCH: the outgoing quality keeps running on its own engine bank
// while the incoming one warms up, and the two are then crossfaded. Nothing is
// reset destructively and nothing is allocated on the audio thread.

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

        //  Audio-thread safe: begins a crossfade to another pre-allocated
        //  oversampler. Latency does NOT change, so the caller has nothing to
        //  re-align; the return value only says whether a fade started.
        bool setQuality (Quality q) noexcept;

        //  Audio-thread safe: starts the colour crossfade if the type changed.
        void setColor (ColorType type) noexcept;

        void setDrive (float drivePercent) noexcept;

        //  Where this band sits in the spectrum. The stage owns the only thing
        //  the caller cannot know - which oversampling factor each engine bank
        //  is currently running at - so it fills that field in itself.
        void setContext (const ColorContext& c) noexcept;

        //  In-place. `behaviorMod`, if given, is a base-rate per-sample linear
        //  gain factor (around 1.0) applied to the engine pre-gain.
        void process (juce::AudioBuffer<float>& buffer,
                      const float* behaviorMod[2] = nullptr) noexcept;

        //  Wet-path latency at the base rate. CONSTANT across all qualities,
        //  and an exact integer.
        float getLatencySamples() const noexcept { return reportedLatency; }

        //  Same number; kept for the callers that size their delay lines by it.
        float getMaxLatencySamples() const noexcept { return reportedLatency; }

        //  One oversampler's own latency, before padding. Diagnostics only.
        float getRawLatencySamples (Quality q) const noexcept
        {
            const int i = juce::jlimit (0, 3, (int) q);
            return oversamplers[i] != nullptr
                       ? (float) oversamplers[i]->getLatencyInSamples() : 0.0f;
        }

        //  Applied internally by process(); exposed for the tests.
        float getCompensationGain() const noexcept
        {
            return engines[activeBank][(size_t) activeColor]->getCompensationGain();
        }

        ColorType getColor() const noexcept { return activeColor; }

        //  The engine currently producing audio. Diagnostics and tests only.
        const ColorEngine& getActiveEngine() const noexcept
        {
            return *engines[activeBank][(size_t) activeColor];
        }
        bool isSwitchingQuality() const noexcept { return qualityFadeLeft > 0; }

    private:
        //  Runs `buffer` through one quality's oversampler, its engine bank and
        //  that path's latency pad. `colourFadeLeft` is passed in so two paths
        //  running in the same block see the same colour-fade position.
        void processPath (int qualityIndex, int bankIndex,
                          juce::AudioBuffer<float>& buffer, int numSamples,
                          const float* const* mod, int colourFadeLeft) noexcept;

        void armBank (int bankIndex, int qualityIndex) noexcept;

        double baseRate = 48000.0;
        int maxBlock = 0;
        int channels = 2;
        float maxLatency = 0.0f;        // worst raw oversampler latency
        float reportedLatency = 0.0f;   // ceil(maxLatency): what every path delivers

        //  Two banks, so the outgoing quality keeps its warm state while the
        //  incoming one runs alongside it. [bank][ColorType]
        std::unique_ptr<ColorEngine> engines[2][4];
        std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[4];   // per Quality

        //  Per-bank pad bringing that path up to reportedLatency, applied in
        //  the oversampled domain where it is always a whole number of samples.
        //  Interpolation type None: this must not filter or smear anything.
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> padDelay[2];
        int padOsSamples[2] = { 0, 0 };

        int activeBank = 0;
        int activeQuality = (int) Quality::high;
        int outgoingQuality = (int) Quality::high;
        int pendingQuality = -1;          // requested while a fade was already running

        ColorType activeColor = ColorType::warm;
        ColorType fadingFrom  = ColorType::warm;
        int fadeSamplesLeft = 0;          // colour fade, at base rate
        int fadeLengthSamples = 1;

        //  Quality fade: a pre-roll during which the incoming path runs at zero
        //  weight while its oversampler, pad and engines fill, then the fade.
        int qualityFadeLeft = 0;
        int qualityFadeTotal = 1;
        int qualityFadeLength = 1;
        int qualityPreRoll = 0;

        juce::AudioBuffer<float> fadeScratch;      // oversampled copy, outgoing colour
        juce::AudioBuffer<float> modScratch;       // oversampled behavior modulation
        juce::AudioBuffer<float> qualityScratch;   // base-rate copy, outgoing quality
        float drivePercent = 25.0f;
        ColorContext bandContext;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NonlinearStage)
    };
}
