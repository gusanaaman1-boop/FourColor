// HARMONIC SPACE - not a reverb. The engine isolates what the saturation
// CREATED and diffuses only that:
//
//   residual = wet - g0*clean - g1*HP(clean)
//
// where (g0, g1) is a slow running least-squares fit over a two-function
// basis: the aligned clean band and its first-order high-pass. A single
// scalar was measured leaving ~37% of the SOURCE in the "residual", because
// the engines' small-signal responses are not flat (IRON's feedback loop and
// BITE's partial de-emphasis are first-order shelves); the high-pass basis
// term absorbs exactly that class of linear colouration, so what remains is
// the genuinely nonlinear part.
//
// The residual feeds a short micro-diffusion (two allpasses into a damped
// comb, no tail), with per-band time ranges - LOW breathes at 13-29 ms,
// HIGH sits tight at 3-7 ms. The LOW band's diffusion is strictly mono;
// the other bands decorrelate L/R by detuned delay lengths.
//
// The fit KEEPS RUNNING while Space is at 0. It used to be skipped with the
// rest of the engine, which meant turning Space up started the estimator from
// nothing: for the first ~200 ms of its 200 ms accumulator the coefficients
// were wrong, and a large slice of the CLEAN source was diffused as if the
// saturation had made it. Only the diffuser is skipped at 0 now - that is where
// the cost is - so the estimator is always converged by the time it is needed.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Core/ParameterIds.h"
#include "TptFilters.h"

namespace fourcolor
{
    class HarmonicSpace
    {
    public:
        HarmonicSpace() = default;

        void prepare (double sampleRate, int maxBlockSize, int numChannels, int bandIndex);
        void reset();

        void setAmount (float amount01) noexcept;

        //  True when the DIFFUSER has work to do. The estimator runs regardless;
        //  process() must be called on every block whatever this returns.
        bool isDiffusing() const noexcept
        {
            return targetAmount > 1.0e-3f || currentAmount > 1.0e-3f;
        }

        //  Diagnostics for the tests: the smoothed linear-fit coefficients.
        float getFitGain (int channel) const noexcept { return fit[channel & 1].g0; }
        float getFitHpGain (int channel) const noexcept { return fit[channel & 1].g1; }

        //  Reads the processed band (`wet`) and the aligned clean band, ADDS
        //  the diffused residual into `wet` in place.
        void process (juce::AudioBuffer<float>& wet,
                      const juce::AudioBuffer<float>& clean, int n) noexcept;

    private:
        struct Diffuser
        {
            std::vector<float> ap1, ap2, comb;
            int ap1Len = 1, ap2Len = 1, combLen = 1;
            int ap1Pos = 0, ap2Pos = 0, combPos = 0;
            dsp::OnePole damping;

            void resize (double sr, float ap1Ms, float ap2Ms, float combMs);
            void clear();

            float process (float x, float feedback) noexcept;
        };

        double rate = 48000.0;
        int channels = 2;
        int band = 0;

        float targetAmount = 0.0f, currentAmount = 0.0f, amountCoeff = 0.0f;

        //  Running two-basis least-squares accumulators, per channel:
        //  s00 = <c,c>, s11 = <h,h>, s01 = <c,h>, r0 = <w,c>, r1 = <w,h>
        //  with c = clean, h = HP(clean), w = wet.
        struct FitState
        {
            float s00 = 0, s11 = 0, s01 = 0, r0 = 0, r1 = 0;

            //  Smoothed solution. g0 starts at 1 because "the wet band is the
            //  clean band" is the correct guess before any audio has been seen;
            //  starting at 0 made the first residual the entire wet signal.
            float g0 = 1.0f, g1 = 0.0f;
        };
        FitState fit[2];
        float accCoeff = 0.0f;
        float coefCoeff = 0.0f;
        bool diffuserPrimed = false;

        dsp::OnePole basisHp[2];      // the high-pass basis filter
        dsp::DcBlocker residualHp[2];
        Diffuser diffuser[2];

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicSpace)
    };
}
