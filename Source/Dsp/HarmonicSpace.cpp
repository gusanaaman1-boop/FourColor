#include "HarmonicSpace.h"

namespace fourcolor
{
    void HarmonicSpace::Diffuser::resize (double sr, float ap1Ms, float ap2Ms, float combMs)
    {
        auto samplesFor = [sr] (float ms) {
            return juce::jmax (1, (int) std::round (ms * 0.001 * sr));
        };

        ap1Len  = samplesFor (ap1Ms);
        ap2Len  = samplesFor (ap2Ms);
        combLen = samplesFor (combMs);

        ap1.assign ((size_t) ap1Len, 0.0f);
        ap2.assign ((size_t) ap2Len, 0.0f);
        comb.assign ((size_t) combLen, 0.0f);
        ap1Pos = ap2Pos = combPos = 0;
    }

    void HarmonicSpace::Diffuser::clear()
    {
        std::fill (ap1.begin(), ap1.end(), 0.0f);
        std::fill (ap2.begin(), ap2.end(), 0.0f);
        std::fill (comb.begin(), comb.end(), 0.0f);
        ap1Pos = ap2Pos = combPos = 0;
        damping.reset();
    }

    float HarmonicSpace::Diffuser::process (float x, float feedback) noexcept
    {
        //  Two Schroeder allpasses (early diffusion)...
        constexpr float k = 0.55f;

        float delayed = ap1[(size_t) ap1Pos];
        float v = x + k * delayed;
        ap1[(size_t) ap1Pos] = v;
        ap1Pos = (ap1Pos + 1) % ap1Len;
        float y = delayed - k * v;

        delayed = ap2[(size_t) ap2Pos];
        v = y + k * delayed;
        ap2[(size_t) ap2Pos] = v;
        ap2Pos = (ap2Pos + 1) % ap2Len;
        y = delayed - k * v;

        //  ...into one damped, bounded feedback comb. No tail: at the maximum
        //  feedback of 0.45 the energy is gone within a few passes.
        const float combOut = comb[(size_t) combPos];
        comb[(size_t) combPos] = y + feedback * damping.process (combOut);
        combPos = (combPos + 1) % combLen;

        return y + combOut;
    }

    void HarmonicSpace::prepare (double sampleRate, int, int numChannels, int bandIndex)
    {
        rate = sampleRate;
        channels = juce::jlimit (1, 2, numChannels);
        band = juce::jlimit (0, 3, bandIndex);

        //  Per-band diffusion clocks (ms), R detuned 13% against L.
        //                          LOW     LMID   HMID   HIGH
        constexpr float ap1Ms[]  = { 13.1f,  7.9f,  4.7f,  2.9f };
        constexpr float ap2Ms[]  = { 19.7f, 11.3f,  6.7f,  4.1f };
        constexpr float combMs[] = { 29.3f, 17.9f, 10.9f,  6.7f };
        constexpr float dampHz[] = { 2000.0f, 3500.0f, 6000.0f, 9000.0f };

        for (int c = 0; c < 2; ++c)
        {
            const float detune = (c == 1 && band != 0) ? 1.13f : 1.0f;
            diffuser[c].resize (sampleRate, ap1Ms[band] * detune,
                                ap2Ms[band] * detune, combMs[band] * detune);
            diffuser[c].damping.setCutoff (sampleRate, dampHz[band]);

            //  Keep rumble out of the LOW band's diffusion; the residual's
            //  harmonics live above the fundamental anyway.
            residualHp[c].prepare (sampleRate, band == 0 ? 60.0f : 20.0f);

            //  High-pass basis corner near the band's default centre, so the
            //  fit can absorb first-order shelf responses in that region.
            constexpr float basisHz[] = { 60.0f, 300.0f, 1800.0f, 8000.0f };
            basisHp[c].setCutoff (sampleRate, basisHz[band]);
        }

        //  ~200 ms accumulator clock for the least-squares fit, ~30 ms for the
        //  amount ramp.
        accCoeff    = 1.0f - std::exp (-1.0f / (0.2f  * (float) sampleRate));
        amountCoeff = 1.0f - std::exp (-1.0f / (0.03f * (float) sampleRate));

        //  The coefficients themselves are smoothed on top of the accumulator,
        //  so a gate slamming shut cannot step the residual in one sample.
        coefCoeff   = 1.0f - std::exp (-1.0f / (0.03f * (float) sampleRate));

        reset();
    }

    void HarmonicSpace::reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            diffuser[c].clear();
            residualHp[c].reset();
            basisHp[c].reset();
            fit[c] = {};
        }
        currentAmount = targetAmount;
        diffuserPrimed = false;
    }

    void HarmonicSpace::setAmount (float amount01) noexcept
    {
        targetAmount = juce::jlimit (0.0f, 1.0f, amount01);
    }

    void HarmonicSpace::process (juce::AudioBuffer<float>& wet,
                                 const juce::AudioBuffer<float>& clean, int n) noexcept
    {
        const int chans = juce::jmin (wet.getNumChannels(), channels);

        float* w[2] = { wet.getWritePointer (0),
                        chans > 1 ? wet.getWritePointer (1) : nullptr };
        const float* cl[2] = { clean.getReadPointer (0),
                               chans > 1 ? clean.getReadPointer (1) : nullptr };

        const bool monoSpace = (band == 0) || chans == 1;

        //  Ridge term, relative to the accumulated energy so it is independent
        //  of level, and the floor below which the fit is frozen rather than
        //  solved from noise. -80 dBFS of running energy is silence.
        constexpr float ridge = 1.0e-4f;
        constexpr float energyFloor = 1.0e-8f;

        //  Below this the diffuser contributes nothing audible, so it is not
        //  run at all - that is the expensive half of this engine.
        constexpr float diffuseFloor = 1.0e-4f;

        for (int i = 0; i < n; ++i)
        {
            currentAmount += amountCoeff * (targetAmount - currentAmount);
            const float a = currentAmount;

            //  Single-knob mapping: send, comb feedback and output level all
            //  ride the one Space value.
            const float feedback = 0.15f + 0.30f * a;
            const float outGain  = 0.9f * a;
            const bool diffusing = a > diffuseFloor;

            //  Two-basis residual per channel (see header). This runs whatever
            //  Space is set to, so the coefficients are converged before anyone
            //  turns the knob up.
            auto residualFor = [this] (int c, float wc, float cc) noexcept
            {
                const float hc = basisHp[c].processHigh (cc);

                auto& f = fit[c];
                f.s00 += accCoeff * (cc * cc - f.s00);
                f.s11 += accCoeff * (hc * hc - f.s11);
                f.s01 += accCoeff * (cc * hc - f.s01);
                f.r0  += accCoeff * (wc * cc - f.r0);
                f.r1  += accCoeff * (wc * hc - f.r1);

                //  Solve only when there is something to solve. In silence the
                //  accumulators decay towards zero and the system becomes
                //  arbitrarily ill-conditioned; holding the last good answer is
                //  both stabler and more musical than chasing noise.
                const float energy = f.s00 + f.s11;
                if (energy > energyFloor)
                {
                    //  Tikhonov: lambda scales with energy, so conditioning is
                    //  level-independent and there is no hard determinant cliff.
                    const float lambda = ridge * energy;
                    const float a00 = f.s00 + lambda;
                    const float a11 = f.s11 + lambda;
                    const float det = a00 * a11 - f.s01 * f.s01;

                    if (det > 1.0e-20f)
                    {
                        const float t0 = (f.r0 * a11 - f.r1 * f.s01) / det;
                        const float t1 = (f.r1 * a00 - f.r0 * f.s01) / det;

                        if (std::isfinite (t0) && std::isfinite (t1))
                        {
                            //  Bound first, then approach: clamping the smoothed
                            //  value would let a spike drag it and then hold it
                            //  pinned at the limit.
                            const float b0 = juce::jlimit (0.0f, 4.0f, t0);
                            const float b1 = juce::jlimit (-4.0f, 4.0f, t1);
                            f.g0 += coefCoeff * (b0 - f.g0);
                            f.g1 += coefCoeff * (b1 - f.g1);
                        }
                    }
                }

                return wc - f.g0 * cc - f.g1 * hc;
            };

            //  With Space down, keep the estimator fed and leave the wet signal
            //  exactly as it was.
            if (! diffusing)
            {
                for (int c = 0; c < chans; ++c)
                    (void) residualFor (c, w[c][i], cl[c][i]);

                //  Empty the delay lines once on the way down, so turning Space
                //  back up cannot flush out a stale tail from minutes ago.
                if (diffuserPrimed)
                {
                    for (auto& d : diffuser)
                        d.clear();
                    diffuserPrimed = false;
                }
                continue;
            }

            diffuserPrimed = true;

            if (monoSpace)
            {
                //  Mono path: average the residual, one diffuser, same signal
                //  to both channels - the sub never widens.
                float residual = 0.0f;
                for (int c = 0; c < chans; ++c)
                    residual += residualFor (c, w[c][i], cl[c][i]);
                residual /= (float) chans;
                residual = residualHp[0].process (residual);

                const float halo = diffuser[0].process (residual * a, feedback) * outGain;
                for (int c = 0; c < chans; ++c)
                    w[c][i] += halo;
            }
            else
            {
                for (int c = 0; c < chans; ++c)
                {
                    float residual = residualHp[c].process (residualFor (c, w[c][i], cl[c][i]));
                    w[c][i] += diffuser[c].process (residual * a, feedback) * outGain;
                }
            }
        }
    }
}
