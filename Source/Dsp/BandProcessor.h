// One band end to end: latency-aligned clean reference, pre-tone, oversampled
// colour, post-tone, band mix, level, mute and bypass. Solo is resolved by the
// owner (it needs to know about all four bands) and arrives here as a gain.
//
// The clean reference is delayed by the oversampler's (fractional) latency
// through a Lagrange line, so Band Mix and Band Bypass never comb-filter.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "../Core/ParameterIds.h"
#include "BehaviorDetector.h"
#include "HarmonicSpace.h"
#include "NonlinearStage.h"
#include "ToneStage.h"

namespace fourcolor
{
    class BandProcessor
    {
    public:
        BandProcessor() = default;

        void prepare (double sampleRate, int maxBlockSize, int numChannels, int bandIndex);
        void reset();

        struct Settings
        {
            ColorType color = ColorType::warm;
            float drivePercent = 25.0f;
            float behavior = 0.0f;        // -1..1 (wired in Phase 5)
            float tone = 0.0f;            // -1..1
            float spacePercent = 0.0f;    // handled by the owner (Phase 6)
            float bandMix = 1.0f;         // 0..1
            float levelDb = 0.0f;
            bool  mute = false;
            bool  bypass = false;
            float soloGain = 1.0f;        // resolved by the owner
            float centreHz = 500.0f;      // band geometric centre, for Tone
        };

        void setSettings (const Settings& s) noexcept;

        //  Returns true if the oversampling factor changed (owner must realign).
        bool setQuality (Quality q) noexcept;

        //  In place. The buffer is this band's split from the crossover.
        void process (juce::AudioBuffer<float>& buffer) noexcept;

        float getLatencySamples() const noexcept { return nonlinear.getLatencySamples(); }
        float getRawLatencySamples (Quality q) const noexcept { return nonlinear.getRawLatencySamples (q); }

        //  Block peak of this band's OUTPUT (post mix/level/mute), for the UI
        //  band meters. Lock-free; the reader exchanges it back to zero.
        float readAndResetOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

        //  The wet path this block, before mix/level - used by HarmonicSpace.
        NonlinearStage& getNonlinearStage() noexcept { return nonlinear; }

    private:
        void updateCleanDelay() noexcept;

        double rate = 48000.0;
        int channels = 2;
        int maxBlock = 0;
        int bandIndex = 0;

        NonlinearStage nonlinear;
        ToneStage tone;
        BehaviorDetector behavior;
        HarmonicSpace space;
        juce::AudioBuffer<float> behaviorMod;   // one stereo-linked curve

        //  Thiran: allpass interpolation, magnitude-flat at fractional delays.
        //  (Lagrange3rd was measured drooping -0.44 dB at 12 kHz on the 59.5
        //  sample delay of 4x quality.)
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Thiran> cleanDelay;
        juce::AudioBuffer<float> cleanBuffer;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> levelGain { 1.0f };
        juce::SmoothedValue<float> mixSmoothed { 1.0f };
        juce::SmoothedValue<float> muteFade { 1.0f };      // 1 = audible
        juce::SmoothedValue<float> bypassFade { 0.0f };    // 1 = bypassed (clean)
        juce::SmoothedValue<float> soloFade { 1.0f };
        juce::SmoothedValue<float> driveSmoothed { 25.0f };

        Settings settings;
        std::atomic<float> outputPeak { 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandProcessor)
    };
}
