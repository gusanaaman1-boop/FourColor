// The whole audio path, host-independent, so the test suite can drive it
// without a plugin wrapper.
//
// Phase 1: input trim -> (processing placeholder: unity) -> latency-aligned
// dry/wet mix -> output trim -> safety. The multiband chain is inserted in
// later phases without changing this public surface.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "../Core/ParameterIds.h"

namespace fourcolor
{
    //  All user parameter values in engine units, pushed once per block from the
    //  APVTS by the processor (or set directly by the tests).
    struct EngineParameters
    {
        float inputDb      = 0.0f;
        float globalDrive  = 50.0f;   // 0..100, 50 = neutral
        float globalTone   = 0.0f;    // -100..100
        bool  autoLevel    = true;
        float mixPercent   = 100.0f;
        float outputDb     = 0.0f;
        Quality quality    = Quality::high;
        bool  bypassed     = false;
        float xoverHz[3]   = { 120.0f, 700.0f, 4500.0f };

        struct Band
        {
            ColorType color  = ColorType::warm;
            float drive      = 25.0f;   // 0..100
            float behavior   = 0.0f;    // -100..100
            float tone       = 0.0f;    // -100..100
            float space      = 0.0f;    // 0..100
            float bandMix    = 100.0f;  // 0..100
            float levelDb    = 0.0f;
            bool  solo       = false;
            bool  mute       = false;
            bool  bypass     = false;
        };
        Band bands[numBands];
    };

    class FourColorEngine
    {
    public:
        FourColorEngine() = default;

        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset();

        //  Called from the audio thread once per block, before process().
        //  Copies values only; must not allocate.
        void setParameters (const EngineParameters& p) noexcept;

        void process (juce::AudioBuffer<float>& buffer) noexcept;

        //  Total latency introduced by the wet path, in samples at the host rate.
        int getLatencySamples() const noexcept { return latencySamples; }

    private:
        void applySafety (juce::AudioBuffer<float>& buffer) noexcept;

        EngineParameters params;

        double sampleRate  = 44100.0;
        int    maxBlock    = 0;
        int    numChannels = 2;
        int    latencySamples = 0;

        juce::AudioBuffer<float> dryBuffer;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGain  { 1.0f };
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGain { 1.0f };
        juce::SmoothedValue<float> mixSmoothed { 1.0f };
        juce::SmoothedValue<float> bypassFade  { 0.0f };   // 0 = active, 1 = bypassed

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourColorEngine)
    };
}
