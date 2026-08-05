#include "AutoLevel.h"

namespace fourcolor
{
    void AutoLevel::prepare (double sampleRate)
    {
        rate = sampleRate;
        measureCoeff = 1.0f - std::exp (-1.0f / (0.5f * (float) sampleRate));
        gainCoeff    = 1.0f - std::exp (-1.0f / (1.5f * (float) sampleRate));
        reset();
    }

    void AutoLevel::reset()
    {
        inPower = outPower = 0.0f;
        gain = 1.0f;
    }

    void AutoLevel::measureInput (const juce::AudioBuffer<float>& buffer, int n, int chans) noexcept
    {
        const float* d[2] = { buffer.getReadPointer (0),
                              chans > 1 ? buffer.getReadPointer (1) : nullptr };

        for (int i = 0; i < n; ++i)
        {
            float p = d[0][i] * d[0][i];
            if (d[1] != nullptr)
                p = 0.5f * (p + d[1][i] * d[1][i]);

            inPower += measureCoeff * (p - inPower);
        }
    }

    void AutoLevel::apply (juce::AudioBuffer<float>& buffer, int n, int chans) noexcept
    {
        float* d[2] = { buffer.getWritePointer (0),
                        chans > 1 ? buffer.getWritePointer (1) : nullptr };

        for (int i = 0; i < n; ++i)
        {
            float p = d[0][i] * d[0][i];
            if (d[1] != nullptr)
                p = 0.5f * (p + d[1][i] * d[1][i]);

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

            d[0][i] *= gain;
            if (d[1] != nullptr)
                d[1][i] *= gain;
        }
    }
}
