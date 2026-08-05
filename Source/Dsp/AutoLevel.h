// Slow, bounded, internal loudness match between the wet chain's input and its
// output. Not a compressor: the measurement window is long, the correction is
// clamped to +/-12 dB, silence is ignored, and the gain moves over seconds.
// It never writes to the visible Output parameter.
//
// The measurement is K-WEIGHTED, following the two-stage filter of ITU-R
// BS.1770: a high shelf that stands in for the head's response, then a
// high-pass that stops sub content counting for more than it is heard. Raw RMS
// treated 40 Hz and 3 kHz as equally loud, which is exactly the mistake that
// makes a saturator seem to change level when it has only changed spectrum.
//
// This is the weighting, not the whole standard: no 400 ms blocks, no gating,
// no true-peak. It is a real-time matcher, and those parts of BS.1770 are for
// measuring a finished master.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace fourcolor
{
    class AutoLevel
    {
    public:
        AutoLevel() = default;

        void prepare (double sampleRate, int maxBlockSize);
        void reset();

        void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

        //  Call with the wet chain's input (post input trim, pre crossover).
        void measureInput (const juce::AudioBuffer<float>& buffer, int n, int chans) noexcept;

        //  Measures the pre-correction output, updates the slow gain, applies it.
        void apply (juce::AudioBuffer<float>& buffer, int n, int chans) noexcept;

        float getCurrentGain() const noexcept { return gain; }

    private:
        //  Direct-form-II transposed biquad. Two of these per channel make the
        //  K weighting; they only ever see the measurement path, never audio.
        struct Biquad
        {
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
            float z1 = 0.0f, z2 = 0.0f;

            void reset() noexcept { z1 = z2 = 0.0f; }

            float process (float x) noexcept
            {
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }

            void setHighShelf (double sampleRate, double freq, double q, double gainDb) noexcept;
            void setHighPass (double sampleRate, double freq, double q) noexcept;
        };

        struct KWeighting
        {
            Biquad shelf, highPass;

            void prepare (double sampleRate) noexcept;
            void reset() noexcept { shelf.reset(); highPass.reset(); }
            float process (float x) noexcept { return highPass.process (shelf.process (x)); }
        };

        double rate = 48000.0;
        bool enabled = true;

        float inPower = 0.0f, outPower = 0.0f;
        float measureCoeff = 0.0f;    // ~500 ms power window
        float gainCoeff = 0.0f;       // ~1.5 s correction glide
        float gain = 1.0f;

        KWeighting inFilters[2], outFilters[2];

        //  BS.1770's relative gate, in miniature. Saturation raises quiet
        //  passages more than loud ones, so on sparse material - a plucked
        //  melody, a vocal with gaps - an ungated mean square weights the gaps
        //  differently in the input and the output and the match comes out
        //  wrong. Only samples the INPUT considers loud enough are counted, and
        //  the same decision gates both accumulators so they stay aligned.
        std::vector<juce::uint8> gateMask;
        float fastInPower = 0.0f, loudReference = 0.0f;
        float fastCoeff = 0.0f, referenceDecay = 0.0f;

        //  -20 dB relative to the running loud reference.
        static constexpr float relativeGate = 0.01f;

        static constexpr float maxCorrection = 3.98f;      // +12 dB
        static constexpr float minCorrection = 0.2512f;    // -12 dB

        //  -60 dBFS of K-weighted power. Below it the gain is held, not chased.
        static constexpr float silenceGate = 1.0e-6f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoLevel)
    };
}
