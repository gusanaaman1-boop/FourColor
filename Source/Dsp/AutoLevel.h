// Slow, bounded, internal loudness match between the wet chain's input and its
// output. Not a compressor: the measurement window is long, the correction is
// clamped to +/-12 dB, silence is ignored, and the gain moves over seconds.
// It never writes to the visible Output parameter.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace fourcolor
{
    class AutoLevel
    {
    public:
        AutoLevel() = default;

        void prepare (double sampleRate);
        void reset();

        void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

        //  Call with the wet chain's input (post input trim, pre crossover).
        void measureInput (const juce::AudioBuffer<float>& buffer, int n, int chans) noexcept;

        //  Measures the pre-correction output, updates the slow gain, applies it.
        void apply (juce::AudioBuffer<float>& buffer, int n, int chans) noexcept;

        float getCurrentGain() const noexcept { return gain; }

    private:
        double rate = 48000.0;
        bool enabled = true;

        float inPower = 0.0f, outPower = 0.0f;
        float measureCoeff = 0.0f;    // ~500 ms power window
        float gainCoeff = 0.0f;       // ~1.5 s correction glide
        float gain = 1.0f;

        static constexpr float maxCorrection = 3.98f;      // +12 dB
        static constexpr float minCorrection = 0.2512f;    // -12 dB
        static constexpr float silenceGate = 1.0e-6f;      // -60 dBFS power

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoLevel)
    };
}
