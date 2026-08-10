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
#include "../Core/StateMigration.h"
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

namespace
{
    //  Writes the golden state fixtures the suite loads back. Regenerating them
    //  is a deliberate act: if a change makes a fixture stop loading, that is
    //  the compatibility break the fixtures exist to catch, and the fix belongs
    //  in the migration chain, not here.
    int writeStateFixtures (const File& dir)
    {
        dir.createDirectory();

        FourColorProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);

        auto write = [&dir] (const String& name, const MemoryBlock& data)
        {
            auto f = dir.getChildFile (name);
            f.replaceWithData (data.getData(), data.getSize());
            std::printf ("  wrote %s (%d bytes)\n", f.getFullPathName().toRawUTF8(),
                         (int) data.getSize());
        };

        //  A handful of presets that between them touch every engine.
        const int wanted[] = { 0, 1, 6, 12, 18, 24 };
        for (int index : wanted)
        {
            if (index >= PresetLibrary::numPresets())
                continue;

            proc.setCurrentProgram (index);
            MemoryBlock block;
            proc.getStateInformation (block);
            write ("preset-" + String (index).paddedLeft ('0', 2) + ".fcstate", block);
        }

        //  A v0 state: exactly what every build before state versioning wrote,
        //  which is the tree with no version property at all.
        proc.setCurrentProgram (1);
        {
            auto tree = proc.apvts.copyState();
            tree.removeProperty (state::versionProperty, nullptr);
            tree.removeProperty (state::editorWidthProperty, nullptr);
            tree.removeProperty (state::editorHeightProperty, nullptr);

            MemoryBlock block;
            MemoryOutputStream stream (block, false);
            tree.writeToStream (stream);
            stream.flush();
            write ("legacy-v0.fcstate", block);
        }

        return 0;
    }
}


namespace
{
    // --- audition pack -----------------------------------------------------------
    //  Six deterministic sources, closed-form so two runs are byte-identical and
    //  so the pack carries no sample licences with it. They are not a substitute
    //  for running your own material through the plug-in in a DAW - they are a
    //  fair, repeatable A/B that isolates one variable at a time.
    enum class Source { sub, eightOhEight, bass, kick, melody, lead, drums, pad, vocal, fullLoop };

    const char* sourceName (Source s)
    {
        switch (s)
        {
            case Source::sub:          return "sub";
            case Source::eightOhEight: return "808";
            case Source::kick:         return "kick";
            case Source::lead:         return "lead";
            case Source::bass:     return "bass";
            case Source::melody:   return "melody";
            case Source::drums:    return "drums";
            case Source::pad:      return "pad";
            case Source::vocal:    return "vocal";
            case Source::fullLoop: return "full-loop";
        }
        return "?";
    }

    double auditionSample (Source src, int64 n, double sr, int channel)
    {
        const double t = (double) n / sr;
        const double bar = std::fmod (t, 2.0);          // 2 s loop, 120 bpm
        const double beat = std::fmod (t, 0.5);
        const double eighth = std::fmod (t, 0.25);

        auto note = [] (double phase, double freq, double decay, double amp)
        {
            return amp * std::exp (-phase * decay)
                       * std::sin (MathConstants<double>::twoPi * freq * phase);
        };

        switch (src)
        {
            case Source::sub:
                //  A held 35 Hz tone with its second partial. This is the file
                //  that answers "does the low end stay put and stay mono".
                return 0.55 * std::sin (MathConstants<double>::twoPi * 35.0 * t)
                     + 0.10 * std::sin (MathConstants<double>::twoPi * 70.0 * t);

            case Source::eightOhEight:
            {
                //  One 808 every bar: pitch envelope from 106 Hz down to 46,
                //  long decay. The single hardest case for BODY, because it is
                //  already deep in the saturator before BODY asks for more.
                const double phase = MathConstants<double>::twoPi
                                         * (46.0 * bar
                                            + (60.0 / 14.0) * (1.0 - std::exp (-bar * 14.0)));
                return 0.70 * std::exp (-bar * 1.1)
                            * (std::sin (phase) + 0.25 * std::sin (2.0 * phase));
            }

            case Source::kick:
            {
                //  Four on the floor, nothing else, so the transient is the
                //  only thing to listen to.
                const double kickF = 110.0 * std::exp (-beat * 22.0) + 48.0;
                return 0.70 * std::exp (-beat * 13.0)
                            * std::sin (MathConstants<double>::twoPi * kickF * beat);
            }

            case Source::lead:
            {
                //  A held, slightly detuned two-oscillator lead with movement:
                //  sustained material with real upper harmonics.
                const double vib = 1.0 + 0.006 * std::sin (MathConstants<double>::twoPi * 4.5 * t);
                const double f = 330.0 * vib * (channel == 0 ? 1.0 : 1.003);
                double v = 0.0;
                for (int h = 1; h <= 10; ++h)
                    v += (0.22 / h) * std::sin (MathConstants<double>::twoPi * f * h * t
                                                + (double) h * 0.4);
                return v * (0.6 + 0.4 * std::sin (MathConstants<double>::twoPi * 0.5 * t));
            }

            case Source::bass:
            {
                //  A rolling bass line: root, fifth, octave, minor seventh.
                const int step = (int) (bar / 0.5);
                const double roots[] = { 55.0, 82.5, 110.0, 98.0 };
                const double f = roots[step & 3];
                //  Two partials plus a touch of third: a synth bass, not a sine.
                return 0.45 * std::exp (-beat * 1.6)
                     * (std::sin (MathConstants<double>::twoPi * f * t)
                        + 0.30 * std::sin (MathConstants<double>::twoPi * f * 2.0 * t)
                        + 0.12 * std::sin (MathConstants<double>::twoPi * f * 3.0 * t));
            }

            case Source::melody:
            {
                //  Plucked eighth notes over a pentatonic figure.
                const int step = (int) (bar / 0.25);
                const double scale[] = { 440.0, 523.25, 587.33, 659.25,
                                         587.33, 523.25, 440.0, 392.0 };
                const double f = scale[step & 7];
                return note (eighth, f, 7.0, 0.34)
                     + note (eighth, f * 2.0, 11.0, 0.12)
                     + note (eighth, f * 3.0, 16.0, 0.05);
            }

            case Source::drums:
            {
                //  Kick on every beat, snare on 2 and 4, hats on eighths.
                const int beatIndex = (int) (bar / 0.5);
                const double kickF = 105.0 * std::exp (-beat * 20.0) + 46.0;
                double v = 0.62 * std::exp (-beat * 12.0)
                                * std::sin (MathConstants<double>::twoPi * kickF * beat);

                if ((beatIndex & 1) == 1)
                {
                    //  Snare: a tuned body plus shaped noise from a fixed LCG,
                    //  so it is noisy but still deterministic.
                    uint32 rng = (uint32) (n * 1664525u + 1013904223u);
                    rng ^= rng >> 13; rng *= 1274126177u; rng ^= rng >> 16;
                    const double noise = ((double) (rng & 0xffff) / 32768.0) - 1.0;
                    v += std::exp (-beat * 26.0)
                         * (0.20 * std::sin (MathConstants<double>::twoPi * 190.0 * beat)
                            + 0.26 * noise);
                }

                {
                    uint32 rng = (uint32) (n * 22695477u + 1u);
                    rng ^= rng >> 15; rng *= 2246822519u; rng ^= rng >> 13;
                    const double noise = ((double) (rng & 0xffff) / 32768.0) - 1.0;
                    v += 0.13 * std::exp (-eighth * 90.0) * noise;
                }

                return v;
            }

            case Source::pad:
            {
                //  Four detuned saw-ish voices, slow swell, slightly different
                //  per channel so the stereo behaviour is exercised.
                const double detune = channel == 0 ? 1.0 : 1.004;
                double v = 0.0;
                const double roots[] = { 110.0, 164.81, 220.0, 277.18 };
                for (double f : roots)
                    for (int h = 1; h <= 6; ++h)
                        v += (0.10 / h) * std::sin (MathConstants<double>::twoPi
                                                        * f * detune * h * t
                                                    + (double) h * 0.7);
                return v * (0.55 + 0.45 * std::sin (MathConstants<double>::twoPi * 0.25 * t));
            }

            case Source::vocal:
            {
                //  A sung vowel: fundamental with vibrato, three formants.
                const double vib = 1.0 + 0.012 * std::sin (MathConstants<double>::twoPi * 5.2 * t);
                const double f0 = 196.0 * vib;
                const double env = 0.5 + 0.5 * std::sin (MathConstants<double>::twoPi * 0.5 * t
                                                         - MathConstants<double>::halfPi);
                double v = 0.0;
                for (int h = 1; h <= 20; ++h)
                {
                    const double f = f0 * h;
                    //  Crude formant emphasis around 700, 1200 and 2600 Hz.
                    double gain = 0.16 / h;
                    for (double formant : { 700.0, 1200.0, 2600.0 })
                        gain += 0.30 / (1.0 + std::pow ((f - formant) / 110.0, 2.0)) / h;
                    v += gain * std::sin (MathConstants<double>::twoPi * f * t + (double) h);
                }
                return v * env * 0.8;
            }

            case Source::fullLoop:
                return 0.55 * auditionSample (Source::drums, n, sr, channel)
                     + 0.55 * auditionSample (Source::bass, n, sr, channel)
                     + 0.35 * auditionSample (Source::melody, n, sr, channel)
                     + 0.30 * auditionSample (Source::pad, n, sr, channel);
        }
        return 0.0;
    }

    struct AuditionSetup
    {
        ColorType color = ColorType::warm;
        float drive = 50.0f;
        float behavior = 0.0f;
        float space = 0.0f;
        bool bypassEngine = false;      // the dry reference
        bool autoLevel = false;
        int  poweredBands = 0xF;        // bit per band; 0 = that band passes clean
        bool foldToMono = false;
    };

    //  Renders one source through one setup and returns interleaved stereo.
    AudioBuffer<float> renderAudition (Source src, const AuditionSetup& setup,
                                       double sr, double seconds)
    {
        constexpr int block = 512;
        const auto total = (int64) (seconds * sr);

        FourColorProcessor proc;
        proc.setPlayConfigDetails (2, 2, sr, block);
        proc.prepareToPlay (sr, block);

        auto set = [&proc] (const String& id, float value)
        {
            if (auto* p = proc.apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        };

        set (param::autoLevel, setup.autoLevel ? 1.0f : 0.0f);
        for (int b = 0; b < numBands; ++b)
        {
            const bool powered = (setup.poweredBands & (1 << b)) != 0;
            set (param::band (b, param::color), (float) (int) setup.color);
            set (param::band (b, param::drive), setup.bypassEngine ? 0.0f : setup.drive);
            set (param::band (b, param::behavior), setup.behavior);
            set (param::band (b, param::space), setup.bypassEngine ? 0.0f : setup.space);
            //  Power off is the band's bypass: the range still passes, clean.
            set (param::band (b, param::bypass), powered ? 0.0f : 1.0f);
        }

        AudioBuffer<float> out (2, (int) total);
        out.clear();

        AudioBuffer<float> io (2, block);
        MidiBuffer midi;
        int64 written = 0;

        //  A short pre-roll so smoothers and the Space estimator are settled
        //  before the first sample anyone hears.
        const int64 preRoll = (int64) (0.5 * sr);

        for (int64 n = -preRoll; written < total; n += block)
        {
            for (int i = 0; i < block; ++i)
                for (int c = 0; c < 2; ++c)
                    io.setSample (c, i, (float) auditionSample (src, n + i, sr, c));

            proc.processBlock (io, midi);

            if (n < 0)
                continue;

            const auto count = (int) jmin ((int64) block, total - written);

            if (setup.foldToMono)
            {
                //  Sum to mono and put the same signal in both channels, which
                //  is what a mono club system does to the mix.
                for (int i = 0; i < count; ++i)
                {
                    const float m = 0.5f * (io.getSample (0, i) + io.getSample (1, i));
                    out.setSample (0, (int) written + i, m);
                    out.setSample (1, (int) written + i, m);
                }
            }
            else
            {
                for (int c = 0; c < 2; ++c)
                    out.copyFrom (c, (int) written, io, c, 0, count);
            }

            written += count;
        }

        return out;
    }

    void normaliseToRms (AudioBuffer<float>& buffer, double targetRms)
    {
        double sumSq = 0.0;
        int64 count = 0;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* d = buffer.getReadPointer (c);
            for (int i = 0; i < buffer.getNumSamples(); ++i) { sumSq += (double) d[i] * d[i]; ++count; }
        }

        const double rms = std::sqrt (sumSq / jmax<int64> (1, count));
        if (rms < 1.0e-9)
            return;

        //  Loudness-matched, then held below full scale so nothing clips on the
        //  way out - a comparison that clips is not a comparison.
        double gain = targetRms / rms;
        const double peak = jmax (buffer.getMagnitude (0, 0, buffer.getNumSamples()),
                                  buffer.getMagnitude (1, 0, buffer.getNumSamples()));
        if (peak * gain > 0.97)
            gain = 0.97 / peak;

        buffer.applyGain ((float) gain);
    }

    bool writeWav (const File& file, const AudioBuffer<float>& buffer, double sr)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        WavAudioFormat format;
        std::unique_ptr<OutputStream> stream (file.createOutputStream().release());
        if (stream == nullptr)
            return false;

        auto writer = format.createWriterFor (stream,
                                              AudioFormatWriterOptions()
                                                  .withSampleRate (sr)
                                                  .withNumChannels (2)
                                                  .withBitsPerSample (24)
                                                  .withSampleFormat (
                                                      AudioFormatWriterOptions::SampleFormat::integral));
        if (writer == nullptr)
            return false;

        return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    int writeAuditionPack (const File& root)
    {
        constexpr double sr = 48000.0;
        constexpr double seconds = 4.0;
        constexpr double targetRms = 0.10;          // about -20 dBFS RMS

        root.createDirectory();
        const Source sources[] = { Source::sub, Source::eightOhEight, Source::bass,
                                   Source::kick, Source::drums, Source::melody,
                                   Source::lead, Source::pad, Source::vocal,
                                   Source::fullLoop };
        const char* colorNames[] = { "warm", "iron", "bite", "fuzz" };
        int written = 0;

        //  Loudness-matched. Everything that compares CHARACTER goes through
        //  here, because a louder file wins a blind A/B whatever it sounds like.
        auto emit = [&] (const String& folder, const String& name,
                         Source src, const AuditionSetup& setup)
        {
            auto buffer = renderAudition (src, setup, sr, seconds);
            normaliseToRms (buffer, targetRms);
            if (writeWav (root.getChildFile (folder).getChildFile (name + ".wav"), buffer, sr))
                ++written;
        };

        //  NOT loudness-matched. Auto Level's whole job is to set the level, so
        //  normalising these two files would erase the only thing being tested.
        auto emitAtTrueLevel = [&] (const String& folder, const String& name,
                                    Source src, const AuditionSetup& setup)
        {
            auto buffer = renderAudition (src, setup, sr, seconds);
            if (writeWav (root.getChildFile (folder).getChildFile (name + ".wav"), buffer, sr))
                ++written;
        };

        //  00 - the dry reference, same loudness as everything else.
        for (auto src : sources)
        {
            AuditionSetup dry;
            dry.bypassEngine = true;
            emit ("00-dry", sourceName (src), src, dry);
        }

        //  01 - the four colours, same drive, on every source. The headline A/B.
        for (auto src : sources)
            for (int c = 0; c < 4; ++c)
            {
                AuditionSetup s;
                s.color = (ColorType) c;
                s.drive = 50.0f;
                emit ("01-colours", String (sourceName (src)) + "-" + colorNames[c], src, s);
            }

        //  02 - drive at 20 / 50 / 80, on the two sources that show it most.
        for (auto src : { Source::bass, Source::drums })
            for (int c = 0; c < 4; ++c)
                for (float drive : { 20.0f, 50.0f, 80.0f })
                {
                    AuditionSetup s;
                    s.color = (ColorType) c;
                    s.drive = drive;
                    emit ("02-drive", String (sourceName (src)) + "-" + colorNames[c]
                              + "-d" + String ((int) drive), src, s);
                }

        //  03 - BODY / centre / ATTACK on drums, where the axis is the point.
        for (int c = 0; c < 4; ++c)
            for (auto behavior : { -100.0f, 0.0f, 100.0f })
            {
                AuditionSetup s;
                s.color = (ColorType) c;
                s.drive = 60.0f;
                s.behavior = behavior;
                const String tag = behavior < 0 ? "body" : (behavior > 0 ? "attack" : "centre");
                emit ("03-behavior", String ("drums-") + colorNames[c] + "-" + tag,
                      Source::drums, s);
            }

        //  04 - Space off against a musical amount, on sustained material.
        for (auto src : { Source::pad, Source::melody })
            for (int c = 0; c < 4; ++c)
                for (float space : { 0.0f, 45.0f })
                {
                    AuditionSetup s;
                    s.color = (ColorType) c;
                    s.drive = 55.0f;
                    s.space = space;
                    emit ("04-space", String (sourceName (src)) + "-" + colorNames[c]
                              + "-space" + String ((int) space), src, s);
                }

        //  05 - Power masks: four, three, two and one coloured band, the rest
        //  passing clean. This is where you hear that Power is not Mute.
        {
            struct Mask { int bits; const char* name; };
            const Mask masks[] = { { 0xF, "4-active" }, { 0xE, "3-active-low-clean" },
                                   { 0xC, "2-active-top" }, { 0x8, "1-active-high" },
                                   { 0x1, "1-active-low" }, { 0x0, "all-clean" } };

            for (auto src : { Source::fullLoop, Source::drums, Source::bass })
                for (auto& m : masks)
                {
                    AuditionSetup s;
                    s.color = ColorType::iron;
                    s.drive = 60.0f;
                    s.poweredBands = m.bits;
                    emit ("05-power", String (sourceName (src)) + "-" + m.name, src, s);
                }
        }

        //  06 - Auto Level off against on, at TRUE level. Compare these two by
        //  switching between them without touching the fader.
        for (auto src : { Source::melody, Source::vocal, Source::fullLoop, Source::bass })
            for (int on = 0; on < 2; ++on)
            {
                AuditionSetup s;
                s.color = ColorType::bite;
                s.drive = 65.0f;
                s.autoLevel = on != 0;
                emitAtTrueLevel ("06-autolevel", String (sourceName (src))
                                     + (on != 0 ? "-al-on" : "-al-off"), src, s);
            }

        //  07 - mono fold on the low end. The sub and the 808 must not lose
        //  weight when the room sums to mono, at any Space setting.
        for (auto src : { Source::sub, Source::eightOhEight, Source::bass })
            for (float space : { 0.0f, 60.0f })
                for (int mono = 0; mono < 2; ++mono)
                {
                    AuditionSetup s;
                    s.color = ColorType::warm;
                    s.drive = 60.0f;
                    s.space = space;
                    s.foldToMono = mono != 0;
                    emit ("07-mono", String (sourceName (src)) + "-space"
                              + String ((int) space) + (mono != 0 ? "-mono" : "-stereo"),
                          src, s);
                }

        std::printf ("  wrote %d files under %s\n", written, root.getFullPathName().toRawUTF8());
        return written > 0 ? 0 : 1;
    }
}

namespace
{
    //  --- preset pack --------------------------------------------------------
    //  Renders each factory preset against the DRY source it was designed for,
    //  loudness-matched, so the only difference you hear is what the preset
    //  does. A preset audition that is not level-matched just tells you which
    //  preset is loudest.
    int writePresetPack (const File& root, const String& onlyCategory)
    {
        constexpr double sr = 48000.0;
        constexpr double seconds = 4.0;
        constexpr double targetRms = 0.10;

        root.createDirectory();

        FourColorProcessor probe;
        const int numPresets = probe.getNumPrograms();
        int written = 0;

        //  Which programme material suits which kind of preset. A drum preset
        //  auditioned on a pad says nothing.
        auto sourcesFor = [] (const String& name) -> std::vector<Source>
        {
            if (name.startsWith ("SUB"))      return { Source::sub, Source::eightOhEight };
            if (name.startsWith ("BASS"))     return { Source::bass, Source::eightOhEight };
            if (name.startsWith ("DRUMS"))    return { Source::drums, Source::kick };
            if (name.startsWith ("LEAD"))     return { Source::lead, Source::melody };
            if (name.startsWith ("PARALLEL")) return { Source::fullLoop, Source::drums };
            return { Source::fullLoop };
        };

        //  One render of a source through a whole preset, by program index.
        auto renderPreset = [&] (int index, Source src)
        {
            const int block = 512;
            const auto total = (int64) (seconds * sr);

            FourColorProcessor proc;
            proc.setPlayConfigDetails (2, 2, sr, block);
            proc.prepareToPlay (sr, block);
            proc.setCurrentProgram (index);

            AudioBuffer<float> out (2, (int) total);
            out.clear();

            AudioBuffer<float> io (2, block);
            MidiBuffer midi;
            int64 written2 = 0;
            const int64 preRoll = (int64) (0.5 * sr);

            for (int64 n = -preRoll; written2 < total; n += block)
            {
                for (int i = 0; i < block; ++i)
                    for (int c = 0; c < 2; ++c)
                        io.setSample (c, i, (float) auditionSample (src, n + i, sr, c));

                proc.processBlock (io, midi);

                if (n < 0)
                    continue;

                const auto count = (int) jmin ((int64) block, total - written2);
                for (int c = 0; c < 2; ++c)
                    out.copyFrom (c, (int) written2, io, c, 0, count);
                written2 += count;
            }

            return out;
        };

        //  The dry reference for each source, once.
        std::vector<Source> allSources = { Source::sub, Source::eightOhEight, Source::bass,
                                           Source::kick, Source::drums, Source::melody,
                                           Source::lead, Source::fullLoop };
        for (auto src : allSources)
        {
            AuditionSetup dry;
            dry.bypassEngine = true;
            auto buffer = renderAudition (src, dry, sr, seconds);
            normaliseToRms (buffer, targetRms);
            if (writeWav (root.getChildFile ("00-dry")
                              .getChildFile (String (sourceName (src)) + ".wav"), buffer, sr))
                ++written;
        }

        for (int i = 0; i < numPresets; ++i)
        {
            const auto name = probe.getProgramName (i);

            //  The signature set is the one with an upper-case task prefix -
            //  "DRUMS - Snare Crack". Everything before it is grouped by its
            //  library category instead, or the folders come out as one per
            //  preset, split on whatever the first word happened to be.
            const auto dash = name.indexOfChar ('-');
            const bool signature = dash > 0
                                && name.substring (0, dash).trim().isNotEmpty()
                                && name.substring (0, dash).trim()
                                       == name.substring (0, dash).trim().toUpperCase();

            if (onlyCategory.equalsIgnoreCase ("signature") && ! signature)
                continue;
            if (onlyCategory.isNotEmpty() && ! onlyCategory.equalsIgnoreCase ("signature")
                && ! name.startsWithIgnoreCase (onlyCategory))
                continue;

            for (auto src : sourcesFor (name))
            {
                auto buffer = renderPreset (i, src);
                normaliseToRms (buffer, targetRms);

                const auto safe = name.replaceCharacters (" /", "__");
                const auto folder = signature
                                      ? name.substring (0, dash).trim()
                                      : String ("z-existing-")
                                            + String (PresetLibrary::category (i));

                if (writeWav (root.getChildFile (folder)
                                  .getChildFile (safe + "__" + sourceName (src) + ".wav"),
                              buffer, sr))
                    ++written;
            }
        }

        std::printf ("  wrote %d files under %s\n", written, root.getFullPathName().toRawUTF8());
        return written > 0 ? 0 : 1;
    }
}

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;

    bool csv = false, fingerprint = false;
    String fixtureDir, auditionDir, presetDir, onlyCategory;

    for (int i = 1; i < argc; ++i)
    {
        const String arg (argv[i]);
        if (arg == "--csv") csv = true;
        else if (arg == "--fingerprint") fingerprint = true;
        else if (arg == "--write-state-fixtures" && i + 1 < argc) fixtureDir = argv[++i];
        else if (arg == "--audition" && i + 1 < argc) auditionDir = argv[++i];
        else if (arg == "--preset-pack" && i + 1 < argc) presetDir = argv[++i];
        else if (arg == "--only" && i + 1 < argc) onlyCategory = argv[++i];
    }

    if (fixtureDir.isNotEmpty())
        return writeStateFixtures (File::getCurrentWorkingDirectory().getChildFile (fixtureDir));

    if (auditionDir.isNotEmpty())
        return writeAuditionPack (File::getCurrentWorkingDirectory().getChildFile (auditionDir));

    if (presetDir.isNotEmpty())
        return writePresetPack (File::getCurrentWorkingDirectory().getChildFile (presetDir),
                                onlyCategory);

    if (! fingerprint)
    {
        std::printf ("usage: FourColorRender --fingerprint [--csv]\n"
                     "       FourColorRender --write-state-fixtures <dir>\n"
                     "       FourColorRender --audition <dir>\n"
                     "       FourColorRender --preset-pack <dir> [--only PREFIX]\n");
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
