#include "ColorEngine.h"

namespace fourcolor
{
    // --- base -----------------------------------------------------------------
    void ColorEngine::prepare (double sampleRate, int numChannels)
    {
        rate = sampleRate;
        channelCount = juce::jlimit (1, maxChannels, numChannels);
        prepareInternals();

        //  10 ms is short enough to feel instant and long enough that a jump
        //  from Drive 0 to Drive 5 cannot step the output.
        engageSmoothed.reset (rate, 0.010);
        setDrive (d01 * 100.0f);
        engageSmoothed.setCurrentAndTargetValue (engageTarget);
        reset();
    }

    void ColorEngine::reset()
    {
        engageSmoothed.setCurrentAndTargetValue (engageTarget);
        resetInternals();
    }

    void ColorEngine::setContext (const ColorContext& c) noexcept
    {
        //  Only act on a real move. setSettings runs every block, and the
        //  crossover smoother delivers a slightly different number every one of
        //  them; recomputing filter coefficients at block rate off a
        //  continuously drifting value is how a crossover ramp turns into a
        //  zipper. A twentieth of a semitone is below anything audible in a
        //  band edge and far above the smoother's per-block step.
        const auto moved = [] (float a, float b)
        {
            return ! (a > 0.0f && b > 0.0f) || std::abs (std::log (a / b)) > 0.003f;
        };

        const bool changed = context.bandIndex != c.bandIndex
                          || std::abs (context.oversampledRate - c.oversampledRate) > 1.0e-6
                          || moved (context.bandLowHz,  c.bandLowHz)
                          || moved (context.bandHighHz, c.bandHighHz)
                          || moved (context.centreHz,   c.centreHz);

        context = c;

        //  Never a reset. A band edge moving is not a reason to empty a
        //  feedback loop, a sag envelope or a DC blocker.
        if (changed)
            contextChanged();
    }

    void ColorEngine::setDrive (float drivePercent) noexcept
    {
        d01 = juce::jlimit (0.0f, 1.0f, drivePercent * 0.01f);
        preGain = juce::Decibels::decibelsToGain (d01 * maxDriveDb());

        //  Drive 0 means a clean pass-through: unity gain, no nonlinear
        //  residual, no gate. Rather than a discontinuity at zero, the whole
        //  engine fades in over the first 5% of the range on a smoothstep, so
        //  the derivative is zero at both ends of that window.
        const float t = juce::jlimit (0.0f, 1.0f, d01 / kEngageDrive01);
        engageTarget = t * t * (3.0f - 2.0f * t);
        engageSmoothed.setTargetValue (engageTarget);

        driveChanged();
        updateCompensation();
    }

    void ColorEngine::updateCompensation() noexcept
    {
        //  Match the level of a -12 dBFS sine through the memoryless curve.
        //
        //  Measured over the FULL cycle, not the positive half: BITE and FUZZ
        //  are asymmetric by design, and half-cycle sampling compensated them
        //  by the wrong amount. The output's mean is removed before taking its
        //  power because the signal path has a DC blocker after the shaper, so
        //  the DC an asymmetric curve produces is not part of what is heard.
        constexpr float ref = 0.25f;
        constexpr int numPoints = 64;

        double outSum = 0.0, outSumSq = 0.0;

        for (int i = 0; i < numPoints; ++i)
        {
            const auto phase = (float) (juce::MathConstants<double>::twoPi
                                            * ((double) i + 0.5) / numPoints);
            const float x = ref * std::sin (phase);
            const float y = staticShape (x * preGain);

            if (! std::isfinite (y))
            {
                compensation = 1.0f;      // a curve that misbehaves gets no make-up
                return;
            }

            outSum   += y;
            outSumSq += (double) y * y;
        }

        const double mean    = outSum / numPoints;
        const double outPower = juce::jmax (0.0, outSumSq / numPoints - mean * mean);
        const double inPower  = 0.5 * (double) ref * ref;   // a zero-mean sine

        if (outPower < 1.0e-12)
        {
            compensation = 1.0f;
            return;
        }

        const auto raw = (float) std::sqrt (inPower / outPower);

        //  Never boost by more than 12 dB or cut by more than 24 dB, and never
        //  let a denormal or a NaN reach the audio path.
        compensation = std::isfinite (raw) ? juce::jlimit (0.0631f, 3.98f, raw) : 1.0f;
    }

    // --- WARM -----------------------------------------------------------------
    namespace
    {
        inline float rationalSoft (float u) noexcept { return u / (1.0f + std::abs (u)); }
    }

    void WarmEngine::prepareInternals()
    {
        for (auto& c : ch)
        {
            //  Slow sag: reacts over ~120 ms, recovers over the same one-pole.
            c.sagEnv.setCutoff (rate, 1.3f);
            c.dc.prepare (rate);
        }
    }

    void WarmEngine::resetInternals()
    {
        for (auto& c : ch) { c.sagEnv.reset(); c.dc.reset(); }
    }

    void WarmEngine::driveChanged() noexcept
    {
        bias     = 0.12f * d01;
        biasOut  = rationalSoft (bias);
        sagDepth = 0.35f * d01;
    }

    float WarmEngine::staticShape (float u) const noexcept
    {
        return rationalSoft (u + bias) - biasOut;
    }

    void WarmEngine::processBlock (float* data, int n, int channel, const float* mod) noexcept
    {
        auto& c = ch[channel];

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            const float g = preGain * (mod != nullptr ? mod[i] : 1.0f);

            //  Sag eases the drive as the (driven) level stays high, which is
            //  the "program-dependent" part of the warmth. It keeps tracking
            //  even while the engine is faded out, so engaging never jumps.
            const float level = c.sagEnv.process (std::abs (x * g));
            const float sag   = 1.0f - sagDepth * juce::jmin (1.0f, level);

            const float u = x * g * sag + bias;
            data[i] = blend (x, c.dc.process (rationalSoft (u) - biasOut));
        }
    }

    // --- IRON -----------------------------------------------------------------
    void IronEngine::updateCoreLoss() noexcept
    {
        //  The loop must return only the BOTTOM of whatever band it is in - that
        //  is what makes the density read as weight rather than as mud. The old
        //  fixed 280 Hz happened to be the LOW MID band's centre; using the
        //  centre generalises it and leaves that band where it was.
        const float corner = juce::jlimit (20.0f, (float) rate * 0.2f, context.centreHz);
        for (auto& c : ch)
            c.coreLoss.setCutoff (rate, corner);
    }

    void IronEngine::prepareInternals()
    {
        for (auto& c : ch)
            c.dc.prepare (rate);

        updateCoreLoss();
    }

    void IronEngine::contextChanged() noexcept
    {
        updateCoreLoss();
    }

    void IronEngine::resetInternals()
    {
        for (auto& c : ch) { c.coreLoss.reset(); c.lastOut = 0.0f; c.dc.reset(); }
    }

    void IronEngine::driveChanged() noexcept
    {
        fbAmount   = 0.45f * d01;
        evenAmount = 0.10f + 0.15f * d01;
    }

    float IronEngine::staticShape (float u) const noexcept
    {
        //  Memoryless approximation of the loop for gain compensation only.
        const float v = u + evenAmount * u * u / (1.0f + std::abs (u));
        return std::tanh (v);
    }

    void IronEngine::processBlock (float* data, int n, int channel, const float* mod) noexcept
    {
        auto& c = ch[channel];

        for (int i = 0; i < n; ++i)
        {
            const float g = preGain * (mod != nullptr ? mod[i] : 1.0f);

            //  Feedback around the saturator: the low-passed previous output is
            //  subtracted at the input, so the curve's operating point depends
            //  on recent history. Loop is stable: tanh is bounded, |fb| < 0.5.
            const float memory = c.coreLoss.process (c.lastOut);
            const float u = data[i] * g - fbAmount * memory;
            const float v = u + evenAmount * u * u / (1.0f + std::abs (u));
            const float y = std::tanh (v);

            c.lastOut = y;
            data[i] = blend (data[i], c.dc.process (y));
        }
    }

    // --- BITE -----------------------------------------------------------------
    void BiteEngine::updateEmphasisFilters() noexcept
    {
        //  Pre-emphasis has to push the TOP OF THIS BAND into the clipper. The
        //  old fixed 1800 Hz was the HIGH MID centre; in the low band it passed
        //  almost nothing and BITE lost the mechanism that defines it.
        const float corner = juce::jlimit (20.0f, (float) rate * 0.2f, context.centreHz);
        for (auto& c : ch)
        {
            c.preHp.setCutoff (rate, corner);
            c.postHp.setCutoff (rate, corner);
        }
    }

    void BiteEngine::prepareInternals()
    {
        for (auto& c : ch)
            c.dc.prepare (rate);

        updateEmphasisFilters();
    }

    void BiteEngine::contextChanged() noexcept
    {
        updateEmphasisFilters();
    }

    void BiteEngine::resetInternals()
    {
        for (auto& c : ch) { c.preHp.reset(); c.postHp.reset(); c.dc.reset(); }
    }

    void BiteEngine::driveChanged() noexcept
    {
        emphasis = 0.6f + 1.2f * d01;
    }

    float BiteEngine::staticShape (float u) const noexcept
    {
        //  Asymmetric exponential diode pair with a deliberately short knee
        //  (hardness 2.2): strong odd AND even content that arrives quickly,
        //  with clear upper partials - the defining difference from WARM.
        constexpr float h = 2.5f;

        if (u >= 0.0f)
            return (1.0f - std::exp (-h * u)) / h;

        return -(1.0f - std::exp (1.3f * h * u)) / (1.3f * h);
    }

    void BiteEngine::processBlock (float* data, int n, int channel, const float* mod) noexcept
    {
        auto& c = ch[channel];

        //  De-emphasis takes back only half the boost: the rest is the
        //  engine's forward, present character. Taking back 100% was measured
        //  to leave BITE with FEWER upper harmonics than WARM.
        const float deemph = 0.5f * emphasis / (1.0f + emphasis);

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            const float g = preGain * (mod != nullptr ? mod[i] : 1.0f);
            float u = x * g;

            //  Internal pre-emphasis: push the upper mids INTO the clipper...
            u += emphasis * c.preHp.processHigh (u);

            float y = staticShape (u);

            //  ...and take the same amount back out afterwards, so the result
            //  is "distorted brighter" rather than just "brighter".
            y -= deemph * c.postHp.processHigh (y);

            data[i] = blend (x, c.dc.process (y));
        }
    }

    // --- FUZZ -----------------------------------------------------------------
    void FuzzEngine::updateGateTimes() noexcept
    {
        //  Half a period of the band centre, so the envelope reads the band's
        //  LEVEL instead of riding its waveform. In the HIGH MID band that lands
        //  near the 0.5 ms this used to be fixed at; in the LOW band it opens
        //  out to about 10 ms, which is the difference between a sputter and a
        //  buzz on a 30 Hz decay.
        const float centre = juce::jlimit (20.0f, 20000.0f, context.centreHz);
        const float attackMs  = juce::jlimit (0.3f, 12.0f, 500.0f / centre);
        const float releaseMs = juce::jlimit (30.0f, 150.0f, 12000.0f / centre);

        envAttack  = 1.0f - std::exp (-1.0f / (attackMs  * 0.001f * (float) rate));
        envRelease = 1.0f - std::exp (-1.0f / (releaseMs * 0.001f * (float) rate));
    }

    void FuzzEngine::prepareInternals()
    {
        for (auto& c : ch)
            c.dc.prepare (rate);

        updateGateTimes();
    }

    void FuzzEngine::contextChanged() noexcept
    {
        updateGateTimes();
    }

    void FuzzEngine::resetInternals()
    {
        for (auto& c : ch) { c.env = 0.0f; c.dc.reset(); }
    }

    void FuzzEngine::driveChanged() noexcept
    {
        rectify       = 0.35f * d01;
        gateThreshold = 0.002f + 0.018f * d01;   // more drive, more sputter
    }

    namespace
    {
        inline float foldOnce (float u) noexcept
        {
            if (u >  1.0f) return  2.0f - u;
            if (u < -1.0f) return -2.0f - u;
            return u;
        }
    }

    float FuzzEngine::staticShape (float u) const noexcept
    {
        float v = (1.0f - rectify) * u + rectify * std::abs (u);
        v = foldOnce (foldOnce (v));
        return juce::jlimit (-1.0f, 1.0f, v);
    }

    void FuzzEngine::processBlock (float* data, int n, int channel, const float* mod) noexcept
    {
        auto& c = ch[channel];
        const float t2 = gateThreshold * gateThreshold;

        for (int i = 0; i < n; ++i)
        {
            const float x = data[i];
            const float g = preGain * (mod != nullptr ? mod[i] : 1.0f);

            //  Gate envelope follows the INPUT, so quiet decays collapse in
            //  that broken, sputtering way instead of sustaining politely.
            const float mag = std::abs (x);
            c.env += (mag > c.env ? envAttack : envRelease) * (mag - c.env);
            const float e2 = c.env * c.env;
            const float gate = e2 / (e2 + t2);

            //  Partial rectification (even harmonics + the ragged texture),
            //  then a double fold, then a clamp.
            float v = (1.0f - rectify) * (x * g) + rectify * std::abs (x * g);
            v = foldOnce (foldOnce (v));
            v = juce::jlimit (-1.0f, 1.0f, v);

            //  At Drive 0 the blend returns x, so the gate is inaudible there
            //  even though its envelope keeps tracking.
            data[i] = blend (x, c.dc.process (v * gate));
        }
    }

    // --- factory --------------------------------------------------------------
    std::unique_ptr<ColorEngine> createColorEngine (ColorType type)
    {
        switch (type)
        {
            case ColorType::warm: return std::make_unique<WarmEngine>();
            case ColorType::iron: return std::make_unique<IronEngine>();
            case ColorType::bite: return std::make_unique<BiteEngine>();
            case ColorType::fuzz: return std::make_unique<FuzzEngine>();
        }
        return std::make_unique<WarmEngine>();
    }
}
