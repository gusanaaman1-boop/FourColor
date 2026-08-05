// FOUR COLOR offline renderer.
//
//   FourColorRender --fingerprint            per-preset RMS / peak / DC, one line each
//   FourColorRender --fingerprint --csv      the same, machine-readable
//
// The fingerprint exists so a DSP change can be checked against every factory
// preset instead of against a feeling. Build it at two commits, diff the two
// outputs, and the difference each preset actually suffered is a number.
//
// The programme material is a fixed, closed-form bed - transients over a low
// drone over a bright top - so two runs of the same binary are byte-identical.

#include <cstdio>

#include <juce_audio_utils/juce_audio_utils.h>

#include "../Core/PresetLibrary.h"
#include "../PluginProcessor.h"

using namespace juce;
using namespace fourcolor;

namespace
{
    constexpr double renderRate = 48000.0;
    constexpr int    renderBlock = 512;
    constexpr int    renderBlocks = 188;      // ~2.0 s

    //  Deterministic stereo programme: a 55 Hz drone with its octave, a kick
    //  every half second, a mid pluck train, and a bright shaker - enough
    //  spread that all four bands see real signal.
    void fillProgramme (AudioBuffer<float>& buffer, int startSample)
    {
        const int n = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const int   s = startSample + i;
            const double t = (double) s / renderRate;

            const double drone = 0.22 * std::sin (MathConstants<double>::twoPi * 55.0 * t)
                               + 0.10 * std::sin (MathConstants<double>::twoPi * 110.0 * t);

            const double kickPhase = std::fmod (t, 0.5);
            const double kick = 0.55 * std::exp (-kickPhase * 26.0)
                                     * std::sin (MathConstants<double>::twoPi * 52.0 * kickPhase);

            const double pluckPhase = std::fmod (t, 0.25);
            const double pluck = 0.20 * std::exp (-pluckPhase * 12.0)
                                      * std::sin (MathConstants<double>::twoPi * 440.0 * pluckPhase);

            const double shakePhase = std::fmod (t, 0.125);
            const double shake = 0.10 * std::exp (-shakePhase * 60.0)
                                      * std::sin (MathConstants<double>::twoPi * 7200.0 * shakePhase);

            const auto left  = (float) (drone + kick + pluck + shake);
            //  A little decorrelation on top so stereo behaviour is exercised.
            const auto right = (float) (drone + kick + pluck * 0.85
                                        + 0.10 * std::exp (-shakePhase * 60.0)
                                               * std::sin (MathConstants<double>::twoPi * 6100.0 * shakePhase));

            buffer.setSample (0, i, left);
            if (buffer.getNumChannels() > 1)
                buffer.setSample (1, i, right);
        }
    }

    struct Fingerprint
    {
        double rmsDb = -200.0, peakDb = -200.0, dc = 0.0;
        float maxBandDrive = 0.0f;
    };

    Fingerprint renderPreset (int presetIndex)
    {
        FourColorProcessor proc;
        proc.setPlayConfigDetails (2, 2, renderRate, renderBlock);
        proc.prepareToPlay (renderRate, renderBlock);
        proc.setCurrentProgram (presetIndex);

        Fingerprint fp;
        for (int b = 0; b < numBands; ++b)
            if (auto* p = proc.apvts.getRawParameterValue (param::band (b, param::drive)))
                fp.maxBandDrive = jmax (fp.maxBandDrive, p->load());

        MidiBuffer midi;
        AudioBuffer<float> buffer (2, renderBlock);

        //  Discard the first half second: smoothers, Auto Level and the Space
        //  estimator all need to settle before the number means anything.
        constexpr int settleBlocks = 47;

        double sumSq = 0.0, sum = 0.0, peak = 0.0;
        int64 counted = 0;

        for (int blk = 0; blk < renderBlocks; ++blk)
        {
            fillProgramme (buffer, blk * renderBlock);
            proc.processBlock (buffer, midi);

            if (blk < settleBlocks)
                continue;

            for (int c = 0; c < 2; ++c)
            {
                auto* d = buffer.getReadPointer (c);
                for (int i = 0; i < renderBlock; ++i)
                {
                    const double v = d[i];
                    sumSq += v * v;
                    sum   += v;
                    peak = jmax (peak, std::abs (v));
                    ++counted;
                }
            }
        }

        if (counted > 0)
        {
            fp.rmsDb  = Decibels::gainToDecibels (std::sqrt (sumSq / (double) counted), -200.0);
            fp.peakDb = Decibels::gainToDecibels (peak, -200.0);
            fp.dc     = sum / (double) counted;
        }
        return fp;
    }
}

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;

    bool csv = false, fingerprint = false;
    for (int i = 1; i < argc; ++i)
    {
        const String arg (argv[i]);
        if (arg == "--csv") csv = true;
        else if (arg == "--fingerprint") fingerprint = true;
    }

    if (! fingerprint)
    {
        std::printf ("usage: FourColorRender --fingerprint [--csv]\n");
        return 2;
    }

    if (csv)
        std::printf ("index,name,category,maxBandDrive,rmsDb,peakDb,dc\n");
    else
        std::printf ("%-4s %-28s %-10s %7s %9s %9s %11s\n",
                     "idx", "preset", "category", "drive", "rms dB", "peak dB", "dc");

    for (int i = 0; i < PresetLibrary::numPresets(); ++i)
    {
        const auto fp = renderPreset (i);
        const auto name = PresetLibrary::name (i);
        const auto category = PresetLibrary::category (i);

        if (csv)
            std::printf ("%d,%s,%s,%.1f,%.6f,%.6f,%.9f\n",
                         i, name.toRawUTF8(), category.toRawUTF8(),
                         fp.maxBandDrive, fp.rmsDb, fp.peakDb, fp.dc);
        else
            std::printf ("%-4d %-28s %-10s %6.1f%% %9.3f %9.3f %11.7f\n",
                         i, name.toRawUTF8(), category.toRawUTF8(),
                         fp.maxBandDrive, fp.rmsDb, fp.peakDb, fp.dc);
    }

    return 0;
}
