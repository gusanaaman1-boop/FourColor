// Small header-only filter primitives used across the DSP.
//
// The state-variable filter is the TPT/ZDF formulation (Zavalishin, "The Art
// of VA Filter Design"), chosen because it stays stable and well-behaved under
// per-block coefficient modulation - which is what makes dragging a crossover
// handle during playback safe.

#pragma once

#include <cmath>

namespace fourcolor::dsp
{
    //  One second-order TPT SVF section. Outputs low / band / high / allpass
    //  from the same state. Coefficients are set explicitly so a caller can
    //  share one coefficient computation across channels and sections.
    struct TptSvf
    {
        //  g = tan(pi * fc / fs), damping = 1/Q. Butterworth: damping = sqrt(2).
        void setCoefficients (float g, float damping) noexcept
        {
            k  = damping;
            a1 = 1.0f / (1.0f + g * (g + damping));
            a2 = g * a1;
            a3 = g * a2;
        }

        void reset() noexcept { ic1 = ic2 = 0.0f; }

        struct Outputs { float low, band, high; };

        Outputs process (float x) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;   // band
            const float v2 = ic2 + a2 * ic1 + a3 * v3;   // low
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return { v2, v1, x - k * v1 - v2 };
        }

        float processLow  (float x) noexcept { return process (x).low; }
        float processHigh (float x) noexcept { return process (x).high; }

        //  Second-order allpass with the section's Q: (s^2 - k w s + w^2) / D.
        float processAllpass (float x) noexcept
        {
            const auto o = process (x);
            return x - 2.0f * k * o.band;
        }

        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.41421356f;
        float ic1 = 0.0f, ic2 = 0.0f;
    };

    inline float svfG (double sampleRate, float cutoffHz) noexcept
    {
        //  Clamp well below Nyquist; tan() explodes at fs/2.
        const float limited = std::fmin (cutoffHz, (float) sampleRate * 0.49f);
        return std::tan (3.14159265358979f * limited / (float) sampleRate);
    }

    //  One-pole low-pass (6 dB/oct), also usable as a smoother.
    struct OnePole
    {
        void setCutoff (double sampleRate, float cutoffHz) noexcept
        {
            const float x = std::exp (-2.0f * 3.14159265f * cutoffHz / (float) sampleRate);
            a = 1.0f - x;
            b = x;
        }

        void reset (float value = 0.0f) noexcept { z = value; }

        float process (float x) noexcept { z = a * x + b * z; return z; }
        float processHigh (float x) noexcept { return x - process (x); }

        float a = 1.0f, b = 0.0f, z = 0.0f;
    };

    //  DC blocker: first-order high-pass around 10 Hz.
    struct DcBlocker
    {
        void prepare (double sampleRate, float cutoffHz = 10.0f) noexcept
        {
            R = 1.0f - 2.0f * 3.14159265f * cutoffHz / (float) sampleRate;
            if (R < 0.9f) R = 0.9f;
        }

        void reset() noexcept { x1 = y1 = 0.0f; }

        float process (float x) noexcept
        {
            const float y = x - x1 + R * y1;
            x1 = x;
            y1 = y;
            return y;
        }

        float R = 0.995f, x1 = 0.0f, y1 = 0.0f;
    };
}
