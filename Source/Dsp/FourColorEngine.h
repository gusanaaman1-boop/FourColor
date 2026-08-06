// The whole audio path, host-independent, so the test suite can drive it
// without a plugin wrapper.
//
//   input trim -> 4-band crossover -> per-band [tone/colour/mix/level] ->
//   recombine -> (global tone + auto level: Phase 7) -> fractional wet
//   alignment -> latency-compensated dry/wet -> output trim -> safety
//
// Latency contract: the oversampler's fractional latency L is rounded UP to
// the integer reported to the host; the wet sum is delayed by (ceil(L) - L)
// through a Lagrange line and the dry path by ceil(L) exactly, so Mix and
// global Bypass are sample-aligned at every quality.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "../Core/ParameterIds.h"
#include "AutoLevel.h"
#include "BandProcessor.h"
#include "Crossover.h"

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

        //  Called with the signal AFTER input trim and BEFORE the crossover -
        //  the level actually entering the colour engines, which is what the
        //  input meter is supposed to show. The processor supplies this so the
        //  DSP has no dependency on the meter type.
        std::function<void (const juce::AudioBuffer<float>&, int, int)> onInputMeasured;

        void process (juce::AudioBuffer<float>& buffer) noexcept;

        //  Total latency of the wet path at the host rate (integer contract).
        int getLatencySamples() const noexcept { return latencySamples; }

        Crossover& getCrossover() noexcept { return crossover; }
        BandProcessor& getBand (int i) noexcept { return bands[i]; }

    private:
        void applySafety (juce::AudioBuffer<float>& buffer) noexcept;
        void updateLatency() noexcept;

        EngineParameters params;

        double sampleRate  = 44100.0;
        int    maxBlock    = 0;
        int    numChannels = 2;
        int    latencySamples = 0;

        Crossover crossover;
        BandProcessor bands[numBands];
        AutoLevel autoLevel;

        juce::AudioBuffer<float> bandBuffers[numBands];
        juce::AudioBuffer<float> dryBuffer;      // TRUE dry (for global bypass)
        juce::AudioBuffer<float> mixDryBuffer;   // allpassed dry (for Mix)
        juce::AudioBuffer<float> apRefBuffer;    // crossover's allpass reference

        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay;
        //  The Mix dry leg is the crossover's ALLPASS REFERENCE, not the raw
        //  input: the wet sum carries the crossover's phase rotation, and
        //  mixing raw dry against it would comb around the crossover points
        //  no matter how exact the delay alignment is. Global bypass still
        //  uses the true dry path.
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> mixDryDelay;
        //  Thiran for magnitude-flat fractional alignment (see BandProcessor.h).
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Thiran> wetAlign;
        float wetAlignDelay = 0.0f;

        //  Global Tone: a tilt around 800 Hz, +/-6 dB per side.
        dsp::OnePole tiltSplit[2];
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> tiltHighGain { 1.0f };

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGain  { 1.0f };
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGain { 1.0f };
        juce::SmoothedValue<float> mixSmoothed { 1.0f };
        juce::SmoothedValue<float> bypassFade  { 0.0f };   // 0 = active, 1 = bypassed

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourColorEngine)
    };
}
