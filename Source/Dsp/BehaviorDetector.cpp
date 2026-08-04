#include "BehaviorDetector.h"

namespace fourcolor
{
    namespace
    {
        inline float onePoleCoeff (double sampleRate, float seconds) noexcept
        {
            return 1.0f - std::exp (-1.0f / (seconds * (float) sampleRate));
        }
    }

    void BehaviorDetector::prepare (double sampleRate, int bandIndex)
    {
        //  Per-band detector clocks. LOW must be slow enough not to track the
        //  waveform of a 40 Hz fundamental (25 ms period); HIGH can be fast.
        //                              LOW    LMID   HMID   HIGH
        constexpr float fastAtkMs[] = { 6.0f,  3.0f,  1.5f,  0.8f };
        constexpr float fastRelMs[] = { 60.0f, 40.0f, 25.0f, 18.0f };
        constexpr float slowAtkMs[] = { 80.0f, 60.0f, 45.0f, 35.0f };
        constexpr float slowRelMs[] = { 200.0f, 150.0f, 120.0f, 100.0f };

        const int b = juce::jlimit (0, 3, bandIndex);
        fastAtk = onePoleCoeff (sampleRate, fastAtkMs[b] * 0.001f);
        fastRel = onePoleCoeff (sampleRate, fastRelMs[b] * 0.001f);
        slowAtk = onePoleCoeff (sampleRate, slowAtkMs[b] * 0.001f);
        slowRel = onePoleCoeff (sampleRate, slowRelMs[b] * 0.001f);

        //  Modulation smoothing: ~3 ms, fast enough to catch a transient's
        //  head, slow enough to be click-free.
        modSmooth = onePoleCoeff (sampleRate, 0.003f);

        reset();
    }

    void BehaviorDetector::reset()
    {
        fastEnv = slowEnv = 0.0f;
        modState = 1.0f;
    }

    void BehaviorDetector::writeModulation (const juce::AudioBuffer<float>& bandInput,
                                            float* modOut, int n) noexcept
    {
        if (! isActive())
        {
            juce::FloatVectorOperations::fill (modOut, 1.0f, n);
            modState = 1.0f;
            return;
        }

        const int chans = bandInput.getNumChannels();
        const float* l = bandInput.getReadPointer (0);
        const float* r = chans > 1 ? bandInput.getReadPointer (1) : nullptr;

        //  +/-6 dB at full transient measure and full amount.
        const float depthDb = maxModDb * amount;

        for (int i = 0; i < n; ++i)
        {
            //  Stereo-linked rectifier: the louder channel drives both, so the
            //  image never leans.
            const float mag = r != nullptr ? juce::jmax (std::abs (l[i]), std::abs (r[i]))
                                           : std::abs (l[i]);

            fastEnv += (mag > fastEnv ? fastAtk : fastRel) * (mag - fastEnv);
            slowEnv += (mag > slowEnv ? slowAtk : slowRel) * (mag - slowEnv);

            //  Bounded transient measure: 0 in steady state, -> 1 when the fast
            //  follower runs ahead of the slow one (i.e. on attacks).
            const float transient = (fastEnv - slowEnv) / (fastEnv + slowEnv + 1.0e-6f);
            const float t = juce::jlimit (0.0f, 1.0f, transient * 2.0f);

            //  dB-linear modulation of the pre-gain, then one-pole smoothing.
            const float targetDb = depthDb * t;
            const float target = std::exp (targetDb * 0.115129254f);   // ln(10)/20
            modState += modSmooth * (target - modState);
            modOut[i] = modState;
        }
    }
}
