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
        //  head, slow enough to be click-free. The residual curve reuses it
        //  when opening and falls far faster when closing - see residualFall.
        modSmooth = onePoleCoeff (sampleRate, 0.003f);
        residualFall = onePoleCoeff (sampleRate, 0.0003f);

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
        modState = residualState = 1.0f;
    }

    void BehaviorDetector::writeModulation (const juce::AudioBuffer<float>& bandInput,
                                            float* driveOut, float* residualOut, int n) noexcept
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

            //  ATTACK is pre-drive only, exactly as before.
            float targetDb = maxModDb * attackAmount * t;
            float residualDb = 0.0f;

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

                //  The transient itself is deliberately excluded: BODY is not
                //  allowed to make the hit louder, that is ATTACK's job.
                //
                //  But it must stand aside for an ONSET, not for any envelope
                //  ripple. ATTACK's `t` is deliberately sensitive - it has to
                //  catch the head of a stick hit - and on sustained material
                //  that sensitivity works against BODY: a vibrato'd vocal read
                //  t ~ 0.3 and a beating pad t ~ 0.2, closing BODY to 42% and
                //  48% on sources that are nothing BUT body. The lift then came
                //  out under target on exactly the material the control exists
                //  for.
                //
                //  So BODY uses a thresholded version of the same measure. Only
                //  the top half counts as an onset, which a real hit reaches
                //  comfortably (the attack-window check still reads 0.00 dB)
                //  while periodic ripple never does.
                const float onset = juce::jlimit (0.0f, 1.0f, (t - 0.5f) * 2.0f);

                //  ...plus a causal guard the envelopes cannot provide. The
                //  fast/slow difference needs a few milliseconds to notice a
                //  hit, and in the LOW band (6 ms fast attack) that is long
                //  enough for BODY - still open from the previous decay - to
                //  reach the front of the next transient. Measured on a kick
                //  train, the hit's crunch rose 3.6 dB under full BODY: not a
                //  level change, but a change to the hit, which is precisely
                //  what this control promises not to do.
                //
                //  The raw magnitude against the slow envelope catches a hit on
                //  its FIRST sample. A steady tone never exceeds about 1.4x its
                //  own slow envelope, so 2x cannot fire on sustained material;
                //  and because the residual smoother closes in 0.3 ms and
                //  reopens over 3 ms, one sample of closure holds long enough
                //  for the envelope measure above to take over.
                const float jump = mag / (slowEnv + 1.0e-6f);
                const float instantOnset = juce::jlimit (0.0f, 1.0f, jump - 2.0f);

                const float bodyOnly = 1.0f - juce::jmax (onset, instantOnset);

                //  Baseline on all sustained material, plus the deficit term.
                const float shape = bodyBaseline
                                  + (1.0f - bodyBaseline) * (deficitDb / bodyRangeDb);

                //  One mask, two mechanisms. They open and close together, so
                //  from the outside BODY is still a single continuous control.
                const float bodyMask = bodyAmount * shape * protection * bodyOnly;

                targetDb   += bodyDriveShareDb    * bodyMask;
                residualDb  = bodyResidualShareDb * bodyMask;
            }

            //  dB-linear modulation, then one-pole smoothing. At amount 0 both
            //  depths are 0, so both targets are unity and the states RELAX to
            //  1.0 through the same smoother instead of snapping - which is
            //  what makes moving the control to zero click-free.
            constexpr float lnTenOverTwenty = 0.115129254f;

            const float driveTarget = std::exp (targetDb * lnTenOverTwenty);
            modState += modSmooth * (driveTarget - modState);
            driveOut[i] = modState;

            const float residualTarget = std::exp (residualDb * lnTenOverTwenty);
            residualState += (residualTarget < residualState ? residualFall : modSmooth)
                                 * (residualTarget - residualState);
            residualOut[i] = residualState;
        }
    }
}
