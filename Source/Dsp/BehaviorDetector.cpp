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

        //  BODY's reference envelope: rises with the material, falls slowly
        //  enough to survive a whole decay. Falling fast would make the
        //  reference chase the decay downwards and the deficit would never
        //  open - BODY would do nothing on exactly the material it exists for.
        //                            LOW    LMID   HMID   HIGH
        constexpr float refRiseMs[] = { 40.0f, 30.0f, 22.0f, 18.0f };
        constexpr float refFallMs[] = { 1200.0f, 1000.0f, 800.0f, 700.0f };
        referenceRise = onePoleCoeff (sampleRate, refRiseMs[b] * 0.001f);
        referenceFall = onePoleCoeff (sampleRate, refFallMs[b] * 0.001f);

        reset();
    }

    void BehaviorDetector::reset()
    {
        fastEnv = slowEnv = referenceEnv = 0.0f;
        modState = 1.0f;
    }

    void BehaviorDetector::writeModulation (const juce::AudioBuffer<float>& bandInput,
                                            float* modOut, int n) noexcept
    {
        const int chans = bandInput.getNumChannels();
        const float* l = bandInput.getReadPointer (0);
        const float* r = chans > 1 ? bandInput.getReadPointer (1) : nullptr;

        //  The two sides are separate mechanisms, so they get separate depths.
        const float attackAmount = juce::jmax (0.0f, amount);
        const float bodyAmount   = juce::jmax (0.0f, -amount);

        for (int i = 0; i < n; ++i)
        {
            //  Stereo-linked rectifier: the louder channel drives both, so the
            //  image never leans.
            const float mag = r != nullptr ? juce::jmax (std::abs (l[i]), std::abs (r[i]))
                                           : std::abs (l[i]);

            fastEnv += (mag > fastEnv ? fastAtk : fastRel) * (mag - fastEnv);
            slowEnv += (mag > slowEnv ? slowAtk : slowRel) * (mag - slowEnv);

            //  The reference BODY measures against: rises with the material,
            //  falls slowly, so it holds the level of the recent passage.
            referenceEnv += (mag > referenceEnv ? referenceRise : referenceFall)
                          * (mag - referenceEnv);

            //  Bounded transient measure: 0 in steady state, -> 1 when the fast
            //  follower runs ahead of the slow one (i.e. on attacks).
            const float transient = (fastEnv - slowEnv) / (fastEnv + slowEnv + 1.0e-6f);
            const float t = juce::jlimit (0.0f, 1.0f, transient * 2.0f);

            float targetDb = maxModDb * attackAmount * t;

            //  --- BODY ------------------------------------------------------
            //  How far below its own recent reference the signal has fallen,
            //  and therefore how much colour it is currently NOT getting.
            if (bodyAmount > 1.0e-4f)
            {
                const float levelDb = juce::Decibels::gainToDecibels (slowEnv, -120.0f);
                const float refDb   = juce::Decibels::gainToDecibels (referenceEnv, -120.0f);

                const float deficitDb = juce::jlimit (0.0f, bodyRangeDb, refDb - levelDb);

                //  Absolute floor, faded rather than gated: a hard gate on a
                //  gain path is a click, and this path is a gain path.
                const float protection =
                    juce::jlimit (0.0f, 1.0f, (levelDb - bodyFloorDb) / bodyFadeDb);

                //  The transient itself is deliberately excluded. BODY is not
                //  allowed to make the hit louder - that is ATTACK's job, and
                //  the brief asks for the initial attack window to stay put.
                const float bodyOnly = 1.0f - t;

                //  Baseline on all sustained material, plus the deficit term.
                const float shape = bodyBaseline
                                  + (1.0f - bodyBaseline) * (deficitDb / bodyRangeDb);

                targetDb += maxBodyDb * bodyAmount * shape * protection * bodyOnly;
            }

            //  dB-linear modulation of the pre-gain, then one-pole smoothing.
            //  At amount 0 both depths are 0, so the target is unity and
            //  modState RELAXES to 1.0 through the same smoother instead of
            //  snapping - which is what makes moving the control to zero
            //  click-free.
            const float target = std::exp (targetDb * 0.115129254f);   // ln(10)/20
            modState += modSmooth * (target - modState);
            modOut[i] = modState;
        }
    }
}
