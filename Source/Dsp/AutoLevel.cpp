#include "AutoLevel.h"

namespace fourcolor
{
    //  RBJ cookbook forms, normalised by a0. BS.1770 tabulates its coefficients
    //  at 48 kHz only; designing from the analogue prototype instead means the
    //  weighting is the same curve at 44.1, 96 and 192 kHz rather than only
    //  correct at one rate.
    void AutoLevel::Biquad::setHighShelf (double sampleRate, double freq,
                                          double q, double gainDb) noexcept
    {
        const double a = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
        const double cosw = std::cos (w0), sinw = std::sin (w0);
        const double alpha = sinw / (2.0 * q);
        const double twoSqrtAlpha = 2.0 * std::sqrt (a) * alpha;

        const double a0 =        (a + 1.0) - (a - 1.0) * cosw + twoSqrtAlpha;
        b0 = (float) ( a * ((a + 1.0) + (a - 1.0) * cosw + twoSqrtAlpha) / a0);
        b1 = (float) (-2.0 * a * ((a - 1.0) + (a + 1.0) * cosw) / a0);
        b2 = (float) ( a * ((a + 1.0) + (a - 1.0) * cosw - twoSqrtAlpha) / a0);
        a1 = (float) ( 2.0 * ((a - 1.0) - (a + 1.0) * cosw) / a0);
        a2 = (float) (      ((a + 1.0) - (a - 1.0) * cosw - twoSqrtAlpha) / a0);
    }

    void AutoLevel::Biquad::setHighPass (double sampleRate, double freq, double q) noexcept
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
        const double cosw = std::cos (w0), sinw = std::sin (w0);
        const double alpha = sinw / (2.0 * q);

        const double a0 = 1.0 + alpha;
        b0 = (float) ( (1.0 + cosw) * 0.5 / a0);
        b1 = (float) (-(1.0 + cosw) / a0);
        b2 = (float) ( (1.0 + cosw) * 0.5 / a0);
        a1 = (float) (-2.0 * cosw / a0);
        a2 = (float) ( (1.0 - alpha) / a0);
    }

    void AutoLevel::KWeighting::prepare (double sampleRate) noexcept
    {
        //  The two BS.1770 stages, by their published parameters.
        shelf.setHighShelf (sampleRate, 1681.97, 0.7071752, 3.99984);
        highPass.setHighPass (sampleRate, 38.13547, 0.5003271);
        reset();
    }

    void AutoLevel::prepare (double sampleRate, int maxBlockSize)
    {
        rate = sampleRate;
        measureCoeff = 1.0f - std::exp (-1.0f / (0.5f * (float) sampleRate));
        gainCoeff    = 1.0f - std::exp (-1.0f / (1.5f * (float) sampleRate));

        //  ~20 ms envelope for the gate decision, and a reference that falls
        //  slowly enough to survive a bar of quiet material.
        fastCoeff      = 1.0f - std::exp (-1.0f / (0.020f * (float) sampleRate));
        referenceDecay = 1.0f - std::exp (-1.0f / (3.0f * (float) sampleRate));

        gateMask.assign ((size_t) juce::jmax (1, maxBlockSize), 1);

        for (auto& f : inFilters)  f.prepare (sampleRate);
        for (auto& f : outFilters) f.prepare (sampleRate);

        reset();
    }

    void AutoLevel::reset()
    {
        inPower = outPower = 0.0f;
        fastInPower = loudReference = 0.0f;
        gain = 1.0f;

        for (auto& f : inFilters)  f.reset();
        for (auto& f : outFilters) f.reset();
    }

    void AutoLevel::measureInput (const juce::AudioBuffer<float>& buffer, int n, int chans) noexcept
    {
        const float* d[2] = { buffer.getReadPointer (0),
                              chans > 1 ? buffer.getReadPointer (1) : nullptr };
        const int used = d[1] != nullptr ? 2 : 1;

        for (int i = 0; i < n; ++i)
        {
            float p = 0.0f;
            for (int c = 0; c < used; ++c)
            {
                const float weighted = inFilters[c].process (d[c][i]);
                p += weighted * weighted;
            }
            p /= (float) used;

            //  Track a fast envelope and a slowly falling reference for the
            //  loudest recent material, then decide whether this sample counts.
            fastInPower += fastCoeff * (p - fastInPower);
            loudReference = juce::jmax (fastInPower,
                                        loudReference - referenceDecay * loudReference);

            const bool counts = fastInPower > loudReference * relativeGate;
            if (i < (int) gateMask.size())
                gateMask[(size_t) i] = counts ? 1 : 0;

            if (counts)
                inPower += measureCoeff * (p - inPower);
        }
    }

    void AutoLevel::apply (juce::AudioBuffer<float>& buffer, int n, int chans) noexcept
    {
        float* d[2] = { buffer.getWritePointer (0),
                        chans > 1 ? buffer.getWritePointer (1) : nullptr };
        const int used = d[1] != nullptr ? 2 : 1;

        for (int i = 0; i < n; ++i)
        {
            float p = 0.0f;
            for (int c = 0; c < used; ++c)
            {
                const float weighted = outFilters[c].process (d[c][i]);
                p += weighted * weighted;
            }
            p /= (float) used;

            //  Gated by the INPUT's decision, taken in measureInput this block.
            const bool counts = i >= (int) gateMask.size() || gateMask[(size_t) i] != 0;
            if (counts)
                outPower += measureCoeff * (p - outPower);

            //  Above the silence gate, glide towards the bounded correction;
            //  in silence, hold (never chase noise floors).
            float target = gain;
            if (! enabled)
                target = 1.0f;
            else if (inPower > silenceGate && outPower > silenceGate)
                target = juce::jlimit (minCorrection, maxCorrection,
                                       std::sqrt (inPower / outPower));

            gain += gainCoeff * (target - gain);

            for (int c = 0; c < used; ++c)
                d[c][i] *= gain;
        }
    }
}
