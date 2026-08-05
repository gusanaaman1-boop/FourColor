// FOUR COLOR measurement and state test suite.
//
//     build/FourColorTests_artefacts/<config>/FourColorTests
//
// Exit code 0 = all checks passed. Every check prints its measured value so a
// failure is diagnosable without a debugger. Sections are appended phase by
// phase, next to the code they measure.

#include <cstdio>
#include <cmath>
#include <functional>
#include <set>
#include <vector>

#include <juce_gui_extra/juce_gui_extra.h>

#include "../Core/ParameterIds.h"
#include "../Dsp/ColorEngine.h"
#include "../Dsp/Crossover.h"
#include "../Dsp/NonlinearStage.h"
#include "../PluginEditor.h"
#include "../PluginProcessor.h"

using namespace juce;
using namespace fourcolor;

// --- allocation tripwire ---------------------------------------------------------
// Replacing global operator new lets the suite prove the audio thread never
// allocates, rather than asserting it in a comment. (Same scheme as the sibling
// DigiMeter suite.)
namespace
{
    std::atomic<bool> trackAllocations { false };
    std::atomic<int>  allocationCount { 0 };

    inline void* trackedAllocate (std::size_t size)
    {
        if (trackAllocations.load (std::memory_order_relaxed))
            allocationCount.fetch_add (1, std::memory_order_relaxed);

        return std::malloc (size == 0 ? 1 : size);
    }

    // MSVC's CRT has no C11 aligned_alloc and its free() cannot release an
    // aligned block, so the aligned pair must go to _aligned_malloc/_aligned_free
    // on Windows. Mixing the families corrupts the heap.
    inline void* alignedAllocate (std::size_t size, std::size_t alignment)
    {
        const auto rounded = ((size + alignment - 1) / alignment) * alignment;

       #if JUCE_WINDOWS
        return _aligned_malloc (rounded, alignment);
       #else
        return std::aligned_alloc (alignment, rounded);
       #endif
    }

    inline void alignedRelease (void* p) noexcept
    {
       #if JUCE_WINDOWS
        _aligned_free (p);
       #else
        std::free (p);
       #endif
    }
}

void* operator new (std::size_t size) { return trackedAllocate (size); }
void* operator new[] (std::size_t size) { return trackedAllocate (size); }
void* operator new (std::size_t size, std::align_val_t alignment)
{
    if (trackAllocations.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);
    return alignedAllocate (size, (std::size_t) alignment);
}
void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    if (trackAllocations.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);
    return alignedAllocate (size, (std::size_t) alignment);
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, std::align_val_t) noexcept { alignedRelease (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { alignedRelease (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { alignedRelease (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { alignedRelease (p); }

// --- tiny check framework --------------------------------------------------------
namespace
{
    int checksRun = 0, checksFailed = 0;

    void check (bool condition, const String& what)
    {
        ++checksRun;
        if (! condition)
        {
            ++checksFailed;
            std::printf ("FAIL  %s\n", what.toRawUTF8());
        }
        else
        {
            std::printf ("  ok  %s\n", what.toRawUTF8());
        }
    }

    //  Performance assertions. A CPU number measured in an unoptimised Debug
    //  build says nothing about the shipped plug-in - it is routinely 5x slower
    //  - so Debug reports the measurement and does not judge it. Lowering the
    //  Release thresholds to accommodate Debug would be tuning a test to pass.
    void checkPerformance ([[maybe_unused]] bool condition, const String& what)
    {
       #if JUCE_DEBUG
        ++checksRun;
        std::printf ("  --  %s   [performance check skipped: Debug build]\n", what.toRawUTF8());
       #else
        check (condition, what);
       #endif
    }

    void checkNear (double measured, double expected, double tolerance, const String& what)
    {
        check (std::abs (measured - expected) <= tolerance,
               what + "  (measured " + String (measured, 6)
                    + ", expected " + String (expected, 6)
                    + " +/- " + String (tolerance, 6) + ")");
    }

    void section (const char* title)
    {
        std::printf ("\n=== %s ===\n", title);
    }

    // --- signal helpers ---------------------------------------------------------
    AudioBuffer<float> makeSine (int channels, int numSamples, double freq, double sampleRate,
                                 float amplitude = 0.5f)
    {
        AudioBuffer<float> buffer (channels, numSamples);
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = amplitude * (float) std::sin (2.0 * MathConstants<double>::pi * freq * i / sampleRate);
        }
        return buffer;
    }

    float peakOf (const AudioBuffer<float>& b)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            peak = jmax (peak, b.getMagnitude (ch, 0, b.getNumSamples()));
        return peak;
    }

    [[maybe_unused]] double rmsOf (const AudioBuffer<float>& b, int channel = 0)
    {
        return b.getRMSLevel (channel, 0, b.getNumSamples());
    }

    bool allFinite (const AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto* d = b.getReadPointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (d[i]))
                    return false;
        }
        return true;
    }

    [[maybe_unused]] double dcOf (const AudioBuffer<float>& b, int channel = 0)
    {
        double sum = 0.0;
        auto* d = b.getReadPointer (channel);
        for (int i = 0; i < b.getNumSamples(); ++i)
            sum += d[i];
        return sum / b.getNumSamples();
    }
}

// ================================================================================
// Phase 1 — skeleton, parameters, state
// ================================================================================
static void testParameterLayout()
{
    section ("Phase 1: parameter layout");

    FourColorProcessor proc;
    auto& apvts = proc.apvts;

    int count = 0;
    for (auto* p : proc.getParameters())
        if (dynamic_cast<RangedAudioParameter*> (p) != nullptr)
            ++count;

    check (count == 51, "51 host-visible parameters (measured " + String (count) + ")");

    // Every documented ID resolves.
    const char* globals[] = { param::input, param::globalDrive, param::globalTone,
                              param::autoLevel, param::mix, param::output,
                              param::quality, param::bypassed,
                              param::xover1, param::xover2, param::xover3 };
    for (auto* id : globals)
        check (apvts.getParameter (id) != nullptr, String ("global parameter exists: ") + id);

    const char* suffixes[] = { param::color, param::drive, param::behavior, param::tone,
                               param::space, param::bandMix, param::level,
                               param::solo, param::mute, param::bypass };
    for (int b = 0; b < numBands; ++b)
        for (auto* s : suffixes)
            check (apvts.getParameter (param::band (b, s)) != nullptr,
                   "band parameter exists: " + param::band (b, s));

    check (proc.getBypassParameter() == apvts.getParameter (param::bypassed),
           "bypass parameter is reported to the host");
}

static void testStateRecall()
{
    section ("Phase 1: state save/recall");

    FourColorProcessor procA;

    // Scatter distinctive values across the parameter space.
    auto set = [&procA] (const String& id, float plain)
    {
        auto* p = procA.apvts.getParameter (id);
        jassert (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    set (param::input, -6.5f);
    set (param::mix, 62.0f);
    set (param::quality, (float) (int) Quality::ultra);
    set (param::xover1, 88.0f);
    set (param::xover3, 8000.0f);
    set (param::band (1, param::drive), 77.0f);
    set (param::band (2, param::color), (float) (int) ColorType::bite);
    set (param::band (3, param::behavior), -40.0f);
    set (param::band (0, param::solo), 1.0f);
    procA.apvts.state.setProperty ("selectedBand", 2, nullptr);

    MemoryBlock state;
    procA.getStateInformation (state);

    FourColorProcessor procB;
    procB.setStateInformation (state.getData(), (int) state.getSize());

    auto get = [&procB] (const String& id)
    {
        auto* p = procB.apvts.getParameter (id);
        return p->convertFrom0to1 (p->getValue());
    };

    checkNear (get (param::input), -6.5, 0.01, "input restored");
    checkNear (get (param::mix), 62.0, 0.1, "mix restored");
    checkNear (get (param::quality), (double) (int) Quality::ultra, 0.01, "quality restored");
    checkNear (get (param::xover1), 88.0, 0.5, "xover1 restored");
    checkNear (get (param::xover3), 8000.0, 20.0, "xover3 restored");
    checkNear (get (param::band (1, param::drive)), 77.0, 0.1, "band 1 drive restored");
    checkNear (get (param::band (2, param::color)), (double) (int) ColorType::bite, 0.01, "band 2 color restored");
    checkNear (get (param::band (3, param::behavior)), -40.0, 0.1, "band 3 behavior restored");
    checkNear (get (param::band (0, param::solo)), 1.0, 0.01, "band 0 solo restored");
    check ((int) procB.apvts.state.getProperty ("selectedBand") == 2, "selectedBand UI property restored");
}

static void testPassthroughAndSafety()
{
    section ("Phase 1: passthrough, bypass, safety");

    const double sr = 48000.0;
    const int    block = 512;

    FourColorProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);

    MidiBuffer midi;

    // Global bypass: output == input delayed by exactly the reported latency.
    {
        auto* bypassParam = proc.apvts.getParameter (param::bypassed);
        bypassParam->setValueNotifyingHost (1.0f);

        //  Continuous sine across blocks so the delay is observable.
        std::vector<float> history;
        AudioBuffer<float> buffer (2, block);
        int sampleIndex = 0;
        double maxError = 0.0;
        const int latency = proc.getLatencySamples();

        for (int blockIndex = 0; blockIndex < 40; ++blockIndex)
        {
            for (int i = 0; i < block; ++i, ++sampleIndex)
            {
                const float v = 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * 997.0 * sampleIndex / sr);
                buffer.setSample (0, i, v);
                buffer.setSample (1, i, v);
                history.push_back (v);
            }

            proc.processBlock (buffer, midi);

            if (blockIndex >= 20)   // smoothing + delays settled
                for (int i = 0; i < block; ++i)
                {
                    const int outIndex = blockIndex * block + i;
                    const int inIndex  = outIndex - latency;
                    if (inIndex >= 0)
                        maxError = jmax (maxError, (double) std::abs (
                            buffer.getSample (0, i) - history[(size_t) inIndex]));
                }
        }

        check (maxError < 1.0e-5, "global bypass = input delayed by reported latency ("
                                      + String (latency) + " smp, max error " + String (maxError, 8) + ")");
        bypassParam->setValueNotifyingHost (0.0f);

        //  Flush the bypass fade AND the filter ring-down before the silence
        //  check: the 10 Hz DC-blocker pole alone needs ~8k samples to decay
        //  below -100 dB (10 blocks left a 1.1e-5 tail).
        AudioBuffer<float> flush (2, block);
        for (int blockIndex = 0; blockIndex < 40; ++blockIndex)
        {
            flush.clear();
            proc.processBlock (flush, midi);
        }
    }

    // Silence in -> silence out, finite always.
    {
        AudioBuffer<float> buffer (2, block);
        buffer.clear();
        proc.processBlock (buffer, midi);
        check (peakOf (buffer) < 1.0e-6f, "silence stays silent (peak "
                                              + String (peakOf (buffer), 9) + ")");
        check (allFinite (buffer), "silence output is finite");
    }

    // NaN/Inf input is scrubbed by the safety stage.
    {
        AudioBuffer<float> buffer (2, block);
        buffer.clear();
        buffer.setSample (0, 5, std::numeric_limits<float>::quiet_NaN());
        buffer.setSample (1, 6, std::numeric_limits<float>::infinity());
        proc.processBlock (buffer, midi);
        check (allFinite (buffer), "NaN/Inf input scrubbed to finite output");
    }

    // A +12 dBFS input does not leave the safety ceiling.
    {
        auto buffer = makeSine (2, block, 200.0, sr, 4.0f);
        proc.processBlock (buffer, midi);
        check (peakOf (buffer) <= 4.001f, "hot input bounded by safety ceiling");
        check (allFinite (buffer), "hot input output is finite");
    }
}

static void testNoAllocationInProcess()
{
    section ("Phase 1: audio thread never allocates");

    const double sr = 48000.0;
    const int    block = 512;

    FourColorProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);

    MidiBuffer midi;
    AudioBuffer<float> buffer (2, block);

    // Warm up (smoothing ramps etc.), then measure.
    for (int i = 0; i < 4; ++i)
    {
        auto sine = makeSine (2, block, 100.0, sr);
        buffer = sine;
        proc.processBlock (buffer, midi);
    }

    allocationCount.store (0);
    trackAllocations.store (true);

    for (int i = 0; i < 50; ++i)
        proc.processBlock (buffer, midi);

    trackAllocations.store (false);

    check (allocationCount.load() == 0,
           "processBlock performs no allocations (counted "
               + String (allocationCount.load()) + ")");
}

// ================================================================================
// Phase 2 — crossover
// ================================================================================
namespace
{
    //  Steady-state RMS of a sine pushed through a processing callback, with the
    //  first half discarded as settling time.
    template <typename Fn>
    double steadySineRms (double freq, double sr, Fn&& processInto)
    {
        const int settle = (int) (sr * 0.25);
        const int measure = (int) (sr * 0.5);
        const int total = settle + measure;

        AudioBuffer<float> in (1, total);
        auto* d = in.getWritePointer (0);
        for (int i = 0; i < total; ++i)
            d[i] = 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * freq * i / sr);

        AudioBuffer<float> out (1, total);
        processInto (in, out);

        double sum = 0.0;
        auto* o = out.getReadPointer (0);
        for (int i = settle; i < total; ++i)
            sum += (double) o[i] * o[i];

        return std::sqrt (sum / measure);
    }
}

static void testCrossoverRecombination()
{
    section ("Phase 2: crossover recombination (magnitude)");

    const double sr = 48000.0;

    Crossover xover;
    xover.prepare (sr, 4096, 1);
    xover.setFrequencies (120.0f, 700.0f, 4500.0f);

    //  Probe from 30 Hz to 18 kHz including the exact crossover points; the
    //  summed band output must be within +/-0.1 dB of the input level.
    const double probes[] = { 30, 60, 120, 240, 350, 700, 1400, 2250, 4500, 9000, 15000, 18000 };
    double worstDeviationDb = 0.0;

    for (double freq : probes)
    {
        xover.reset();

        const double rms = steadySineRms (freq, sr,
            [&] (AudioBuffer<float>& in, AudioBuffer<float>& out)
            {
                const int n = in.getNumSamples();
                AudioBuffer<float> bands[numBands];
                for (auto& b : bands) { b.setSize (1, n); b.clear(); }

                xover.process (in, bands);

                out.clear();
                for (auto& b : bands)
                    out.addFrom (0, 0, b, 0, 0, n);
            });

        const double devDb = Decibels::gainToDecibels (rms / (0.5 / std::sqrt (2.0)));
        worstDeviationDb = jmax (worstDeviationDb, std::abs (devDb));
        checkNear (devDb, 0.0, 0.1, "recombined level flat at " + String (freq, 0) + " Hz");
    }

    std::printf ("      worst recombination deviation: %.5f dB\n", worstDeviationDb);
}

static void testCrossoverNull()
{
    section ("Phase 2: crossover null against allpass reference");

    const double sr = 48000.0;
    const int n = 48000;

    Crossover xover;
    xover.prepare (sr, n, 1);
    xover.setFrequencies (120.0f, 700.0f, 4500.0f);
    xover.reset();

    //  Pink-ish broadband test signal: sum of many sines with random phases.
    AudioBuffer<float> in (1, n);
    in.clear();
    Random rng (42);
    for (int k = 0; k < 60; ++k)
    {
        const double freq = 25.0 * std::pow (1.12, k);
        if (freq > 20000.0) break;
        const double phase = rng.nextDouble() * 2.0 * MathConstants<double>::pi;
        auto* d = in.getWritePointer (0);
        for (int i = 0; i < n; ++i)
            d[i] += (float) (0.02 * std::sin (2.0 * MathConstants<double>::pi * freq * i / sr + phase));
    }

    AudioBuffer<float> bands[numBands], ref (1, n);
    for (auto& b : bands) { b.setSize (1, n); b.clear(); }

    xover.process (in, bands, &ref);

    //  Sum of bands minus the analytic allpass reference: proves both magnitude
    //  AND phase agreement. The two paths compute the same transfer function
    //  through different float32 filter chains, so the residual floor is float
    //  rounding noise, not zero: worst-sample is checked against -95 dB and the
    //  RMS (the standard null-test measure) against -110 dB.
    double maxResidual = 0.0, signalPeak = 0.0, residualPower = 0.0, signalPower = 0.0;
    for (int i = 0; i < n; ++i)
    {
        double sum = 0.0;
        for (auto& b : bands)
            sum += b.getSample (0, i);

        const double residual = sum - (double) ref.getSample (0, i);
        const double sig      = (double) in.getSample (0, i);

        maxResidual   = jmax (maxResidual, std::abs (residual));
        signalPeak    = jmax (signalPeak, std::abs (sig));
        residualPower += residual * residual;
        signalPower   += sig * sig;
    }

    //  gainToDecibels clamps at -100 dB by default, which would hide how deep
    //  the null actually is; extend the floor.
    const double nullPeakDb = Decibels::gainToDecibels (maxResidual / jmax (1.0e-12, signalPeak), -300.0);
    const double nullRmsDb  = Decibels::gainToDecibels (
                                  std::sqrt (residualPower / jmax (1.0e-24, signalPower)), -300.0);

    //  Criterion calibration: the residual measured here is ~4e-7 RMS against a
    //  0.11 RMS signal (-109 dB), i.e. float32 rounding through 14 cascaded
    //  sections - the two paths are algebraically identical. -100 dB is the
    //  conventional "complete null" bar and leaves margin above the noise floor
    //  without being unreachable for float arithmetic.
    check (nullPeakDb < -95.0, "band sum nulls against allpass reference, worst sample ("
                                   + String (nullPeakDb, 1) + " dB)");
    check (nullRmsDb < -100.0, "band sum nulls against allpass reference, RMS ("
                                   + String (nullRmsDb, 1) + " dB)");
}

static void testCrossoverSpacingAndAutomation()
{
    section ("Phase 2: crossover spacing + click-free automation");

    const double sr = 48000.0;

    Crossover xover;
    xover.prepare (sr, 512, 1);

    //  Spacing: ask for crossed-over cuts, verify enforcement.
    xover.setFrequencies (400.0f, 300.0f, 1500.0f);
    xover.reset();
    check (xover.getCurrentFrequency (0) <= 300.0f / Crossover::minRatio + 0.1f,
           "f1 pushed below f2/ratio (got " + String (xover.getCurrentFrequency (0), 1) + " Hz)");
    check (xover.getCurrentFrequency (2) >= 300.0f * Crossover::minRatio - 0.1f,
           "f3 pushed above f2*ratio (got " + String (xover.getCurrentFrequency (2), 1) + " Hz)");

    //  Automation: sweep f2 hard while a sine runs; the summed output must stay
    //  finite and free of sample-to-sample jumps (clicks).
    xover.setFrequencies (120.0f, 700.0f, 4500.0f);
    xover.reset();

    const int block = 512, numBlocks = 100;
    AudioBuffer<float> bands[numBands];
    for (auto& b : bands) b.setSize (1, block);

    double maxStep = 0.0;
    float prev = 0.0f;
    int sampleIndex = 0;

    for (int blk = 0; blk < numBlocks; ++blk)
    {
        //  Zig-zag f2 across most of its range every 25 blocks.
        const float f2 = 300.0f + 2000.0f * (0.5f + 0.5f * std::sin (blk * 0.25f));
        xover.setFrequencies (120.0f, f2, 4500.0f);

        AudioBuffer<float> in (1, block);
        auto* d = in.getWritePointer (0);
        for (int i = 0; i < block; ++i, ++sampleIndex)
            d[i] = 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * 440.0 * sampleIndex / sr);

        for (auto& b : bands) b.clear();
        xover.process (in, bands);

        for (int i = 0; i < block; ++i)
        {
            float sum = 0.0f;
            for (auto& b : bands)
                sum += b.getSample (0, i);

            maxStep = jmax (maxStep, (double) std::abs (sum - prev));
            prev = sum;

            if (! std::isfinite (sum))
            {
                check (false, "crossover output stayed finite during automation");
                return;
            }
        }
    }

    //  A 440 Hz sine at 0.5 changes at most ~0.029/sample on its own; allow
    //  generous headroom for the moving filters but fail on genuine clicks.
    check (maxStep < 0.1, "no clicks while sweeping f2 (max step " + String (maxStep, 4) + ")");
}

// ================================================================================
// Phase 3 — colour engines + oversampling
// ================================================================================
namespace
{
    //  Goertzel magnitude of one frequency in a windowless steady-state segment.
    double goertzel (const float* data, int n, double freq, double sr)
    {
        const double w = 2.0 * MathConstants<double>::pi * freq / sr;
        const double coeff = 2.0 * std::cos (w);
        double s0 = 0.0, s1 = 0.0, s2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            s0 = data[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const double real = s1 - s2 * std::cos (w);
        const double imag = s2 * std::sin (w);
        return 2.0 * std::sqrt (real * real + imag * imag) / n;
    }

    //  Harmonic profile of an engine at a given drive: amplitudes of h1..h8
    //  for a 220.5 Hz sine at -12 dBFS, measured after settling.
    struct HarmonicProfile
    {
        double h[8] {};
        double dc = 0.0;
        double rms = 0.0;

        //  Normalised (h1 = 1) log-spectral distance to another profile.
        double distanceTo (const HarmonicProfile& other) const
        {
            double sum = 0.0;
            for (int k = 1; k < 8; ++k)   // skip the fundamental itself
            {
                const double a = Decibels::gainToDecibels (h[k] / jmax (1.0e-12, h[0]), -120.0);
                const double b = Decibels::gainToDecibels (other.h[k] / jmax (1.0e-12, other.h[0]), -120.0);
                sum += (a - b) * (a - b);
            }
            return std::sqrt (sum / 7.0);
        }
    };

    HarmonicProfile measureEngine (ColorType type, float drive, double sr = 48000.0)
    {
        //  Chosen so 8 harmonics fit under Nyquist with margin.
        const double f0 = 220.5;
        const int settle = (int) sr / 2, measure = (int) sr;

        auto engine = createColorEngine (type);
        engine->prepare (sr, 1);
        engine->setDrive (drive);

        AudioBuffer<float> buf (1, settle + measure);
        auto* d = buf.getWritePointer (0);
        for (int i = 0; i < settle + measure; ++i)
            d[i] = 0.25f * (float) std::sin (2.0 * MathConstants<double>::pi * f0 * i / sr);

        engine->processBlock (d, settle + measure, 0, nullptr);

        HarmonicProfile p;
        const float* m = d + settle;
        for (int k = 0; k < 8; ++k)
            p.h[k] = goertzel (m, measure, f0 * (k + 1), sr);

        double sum = 0.0, sq = 0.0;
        for (int i = 0; i < measure; ++i) { sum += m[i]; sq += (double) m[i] * m[i]; }
        p.dc  = sum / measure;
        p.rms = std::sqrt (sq / measure);
        return p;
    }
}

static void testColorEnginesDiffer()
{
    section ("Phase 3: engines are genuinely different");

    HarmonicProfile profiles[4];
    const char* names[] = { "WARM", "IRON", "BITE", "FUZZ" };

    for (int e = 0; e < 4; ++e)
    {
        profiles[e] = measureEngine ((ColorType) e, 50.0f);

        std::printf ("      %s  h1..h8 (dB rel h1):", names[e]);
        for (int k = 0; k < 8; ++k)
            std::printf (" %6.1f", Decibels::gainToDecibels (profiles[e].h[k] / jmax (1.0e-12, profiles[e].h[0]), -120.0));
        std::printf ("   dc %.6f  rms %.4f\n", profiles[e].dc, profiles[e].rms);

        check (std::abs (profiles[e].dc) < 1.0e-3,
               String (names[e]) + " has no DC offset (" + String (profiles[e].dc, 7) + ")");
        check (profiles[e].h[1] + profiles[e].h[2] > 1.0e-4,
               String (names[e]) + " actually distorts at drive 50");
    }

    //  Pairwise log-spectral distance: engines must differ in SPECTRUM, not
    //  just level. 3 dB average per-harmonic difference is clearly audible.
    for (int a = 0; a < 4; ++a)
        for (int b = a + 1; b < 4; ++b)
        {
            const double dist = profiles[a].distanceTo (profiles[b]);
            check (dist > 3.0, String (names[a]) + " vs " + names[b]
                                   + " spectra differ (distance " + String (dist, 2) + " dB)");
        }

    //  Character assertions from the design goals:
    //  WARM keeps upper harmonics well below BITE's.
    const double warmHigh = (profiles[0].h[5] + profiles[0].h[6] + profiles[0].h[7]) / profiles[0].h[0];
    const double biteHigh = (profiles[2].h[5] + profiles[2].h[6] + profiles[2].h[7]) / profiles[2].h[0];
    check (biteHigh > 2.0 * warmHigh, "BITE has far more upper harmonics than WARM ("
               + String (Decibels::gainToDecibels (biteHigh / jmax (1.0e-12, warmHigh)), 1) + " dB more)");

    //  Engines respond to drive: harmonic content grows.
    for (int e = 0; e < 4; ++e)
    {
        const auto lo = measureEngine ((ColorType) e, 10.0f);
        const auto hi = measureEngine ((ColorType) e, 90.0f);
        const double loH = (lo.h[1] + lo.h[2]) / jmax (1.0e-12, lo.h[0]);
        const double hiH = (hi.h[1] + hi.h[2]) / jmax (1.0e-12, hi.h[0]);
        check (hiH > 2.0 * loH, String (names[e]) + " harmonic content grows with drive");
    }
}

static void testColorLoudnessMatch()
{
    section ("Phase 3: loudness stays in a window across engines and drives");

    //  With static compensation, RMS out should stay within roughly +/-4 dB of
    //  RMS in for a -12 dBFS sine across all engines and the whole drive range.
    //  (Auto Level narrows this further in Phase 7.)
    const double sr = 48000.0;
    const double refRms = 0.25 / std::sqrt (2.0);
    const char* names[] = { "WARM", "IRON", "BITE", "FUZZ" };

    for (int e = 0; e < 4; ++e)
        for (float drive : { 5.0f, 25.0f, 50.0f, 75.0f, 100.0f })
        {
            NonlinearStage stage;
            stage.prepare (sr, 512, 1);
            stage.setQuality (Quality::high);
            stage.setColor ((ColorType) e);
            stage.setDrive (drive);
            stage.reset();

            double sq = 0.0;
            int counted = 0;
            int sampleIndex = 0;

            for (int blk = 0; blk < 100; ++blk)
            {
                AudioBuffer<float> buf (1, 512);
                auto* d = buf.getWritePointer (0);
                for (int i = 0; i < 512; ++i, ++sampleIndex)
                    d[i] = 0.25f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.5 * sampleIndex / sr);

                stage.process (buf);

                if (blk >= 50)
                {
                    for (int i = 0; i < 512; ++i)
                        sq += (double) d[i] * d[i];
                    counted += 512;
                }
            }

            const double outRms = std::sqrt (sq / counted);
            const double devDb = Decibels::gainToDecibels (outRms / refRms, -120.0);
            check (std::abs (devDb) < 4.0,
                   String (names[e]) + " drive " + String ((int) drive)
                       + " level deviation " + String (devDb, 2) + " dB (limit 4)");
        }
}

static void testAliasingByQuality()
{
    section ("Phase 3: aliasing vs quality (FUZZ, worst case)");

    //  A hot sine through FUZZ at drive 85. Harmonics land at k*f0; everything
    //  else above the analysis floor is aliasing. The alias level must drop
    //  substantially from 1x to 4x. f0 sits exactly on an FFT bin and the
    //  window is 4-term Blackman-Harris (-92 dB sidelobes): with a Hann window
    //  and an off-bin f0, sidelobe leakage at -31 dB was measured masquerading
    //  as an alias floor.
    const double sr = 48000.0;
    const int fftOrder = 15, fftSize = 1 << fftOrder;   // 32768
    const double f0 = 1700.0 * sr / fftSize;            // 2490.2 Hz, bin-centred

    double aliasDb[4] = {};

    for (int q = 0; q < 4; ++q)
    {
        NonlinearStage stage;
        stage.prepare (sr, 512, 1);
        stage.setQuality ((Quality) q);
        stage.setColor (ColorType::fuzz);
        stage.setDrive (85.0f);
        stage.reset();

        //  Render enough audio to settle, keep the last fftSize samples.
        std::vector<float> tail ((size_t) fftSize, 0.0f);
        int written = 0, sampleIndex = 0;
        const int totalBlocks = 140;   // ~1.5 s

        for (int blk = 0; blk < totalBlocks; ++blk)
        {
            AudioBuffer<float> buf (1, 512);
            auto* d = buf.getWritePointer (0);
            for (int i = 0; i < 512; ++i, ++sampleIndex)
                d[i] = 0.7f * (float) std::sin (2.0 * MathConstants<double>::pi * f0 * sampleIndex / sr);

            stage.process (buf);

            for (int i = 0; i < 512; ++i)
            {
                tail[(size_t) (written % fftSize)] = d[i];
                ++written;
            }
        }

        //  4-term Blackman-Harris window + FFT.
        juce::dsp::FFT fft (fftOrder);
        std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
        for (int i = 0; i < fftSize; ++i)
        {
            const double t = 2.0 * MathConstants<double>::pi * i / (fftSize - 1);
            const float w = (float) (0.35875 - 0.48829 * std::cos (t)
                                   + 0.14128 * std::cos (2.0 * t)
                                   - 0.01168 * std::cos (3.0 * t));
            fftData[(size_t) i] = tail[(size_t) ((written + i) % fftSize)] * w;
        }
        fft.performRealOnlyForwardTransform (fftData.data());

        auto binMag = [&] (int bin)
        {
            const float re = fftData[(size_t) (2 * bin)];
            const float im = fftData[(size_t) (2 * bin + 1)];
            return std::sqrt ((double) re * re + (double) im * im);
        };

        //  Harmonic bins (+/-6 bins for the BH window main lobe), fundamental.
        const double binHz = sr / fftSize;
        std::vector<bool> isHarmonic ((size_t) fftSize / 2, false);
        for (int k = 1; k * f0 < sr / 2.0; ++k)
        {
            const int centre = (int) std::round (k * f0 / binHz);
            for (int b = centre - 6; b <= centre + 6; ++b)
                if (b >= 0 && b < fftSize / 2)
                    isHarmonic[(size_t) b] = true;
        }
        //  Ignore DC and the sub-30 Hz region (window leakage).
        for (int b = 0; b < (int) (30.0 / binHz) + 1; ++b)
            isHarmonic[(size_t) b] = true;

        const double fundamental = binMag ((int) std::round (f0 / binHz));

        //  Two measures. The criterion applies to the AUDIBLE band (< 20 kHz):
        //  a half-band decimator's transition band straddles Nyquist, so
        //  content just above 24 kHz folds to just below it with partial
        //  attenuation regardless of the oversampling factor. That near-24k
        //  fold (measured at 23.1 kHz here) is inherent to half-band FIR
        //  oversampling, is above audibility, and is reported separately.
        double worstAlias = 0.0, worstFull = 0.0;
        int worstBin = 0, worstFullBin = 0;
        const int audibleLimitBin = (int) (20000.0 / binHz);

        for (int b = 1; b < fftSize / 2; ++b)
        {
            if (isHarmonic[(size_t) b])
                continue;

            const double m = binMag (b);
            if (m > worstFull)      { worstFull = m; worstFullBin = b; }
            if (b <= audibleLimitBin && m > worstAlias) { worstAlias = m; worstBin = b; }
        }

        aliasDb[q] = Decibels::gainToDecibels (worstAlias / jmax (1.0e-12, fundamental), -200.0);
        const double fullDb = Decibels::gainToDecibels (worstFull / jmax (1.0e-12, fundamental), -200.0);
        std::printf ("      %dx: worst audible alias %.1f dB (at %.0f Hz); full band %.1f dB (at %.0f Hz)\n",
                     1 << q, -aliasDb[q], worstBin * binHz, -fullDb, worstFullBin * binHz);
    }

    //  Criteria calibrated to the physics of the worst case: a hard-gated fuzz
    //  spectrum decays roughly 1/k, so even a perfect decimator leaves the
    //  fold-back of whatever the transition band passes. Each quality step must
    //  clearly improve (>3 dB), 4x must hold -40 dB and 8x -55 dB on THIS
    //  worst-case signal. Typical material is far cleaner - measured below.
    check (aliasDb[1] < aliasDb[0] - 3.0, "2x clearly reduces worst-case alias vs 1x ("
               + String (aliasDb[0] - aliasDb[1], 1) + " dB)");
    check (aliasDb[2] < aliasDb[1] - 3.0, "4x clearly reduces worst-case alias vs 2x ("
               + String (aliasDb[1] - aliasDb[2], 1) + " dB)");
    check (aliasDb[3] < aliasDb[2] - 3.0, "8x clearly reduces worst-case alias vs 4x ("
               + String (aliasDb[2] - aliasDb[3], 1) + " dB)");
    check (aliasDb[2] < -40.0, "4x (default) worst-case audible alias below -40 dB ("
               + String (aliasDb[2], 1) + " dB)");
    check (aliasDb[3] < -55.0, "8x (Ultra) worst-case audible alias below -55 dB ("
               + String (aliasDb[3], 1) + " dB)");
}

static void testAliasingTypicalCase()
{
    section ("Phase 3: aliasing at default quality, typical material (BITE)");

    //  BITE at drive 60 on a -12 dBFS 2490 Hz sine: the kind of signal the
    //  default quality actually meets. Audible-band alias must clear -60 dB.
    const double sr = 48000.0;
    const int fftOrder = 15, fftSize = 1 << fftOrder;
    const double f0 = 1700.0 * sr / fftSize;

    NonlinearStage stage;
    stage.prepare (sr, 512, 1);
    stage.setQuality (Quality::high);
    stage.setColor (ColorType::bite);
    stage.setDrive (60.0f);
    stage.reset();

    std::vector<float> tail ((size_t) fftSize, 0.0f);
    int written = 0, sampleIndex = 0;

    for (int blk = 0; blk < 140; ++blk)
    {
        AudioBuffer<float> buf (1, 512);
        auto* d = buf.getWritePointer (0);
        for (int i = 0; i < 512; ++i, ++sampleIndex)
            d[i] = 0.25f * (float) std::sin (2.0 * MathConstants<double>::pi * f0 * sampleIndex / sr);

        stage.process (buf);

        for (int i = 0; i < 512; ++i)
            tail[(size_t) (written++ % fftSize)] = d[i];
    }

    juce::dsp::FFT fft (fftOrder);
    std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
    for (int i = 0; i < fftSize; ++i)
    {
        const double t = 2.0 * MathConstants<double>::pi * i / (fftSize - 1);
        const float w = (float) (0.35875 - 0.48829 * std::cos (t)
                               + 0.14128 * std::cos (2.0 * t)
                               - 0.01168 * std::cos (3.0 * t));
        fftData[(size_t) i] = tail[(size_t) ((written + i) % fftSize)] * w;
    }
    fft.performRealOnlyForwardTransform (fftData.data());

    auto binMag = [&] (int bin)
    {
        const float re = fftData[(size_t) (2 * bin)];
        const float im = fftData[(size_t) (2 * bin + 1)];
        return std::sqrt ((double) re * re + (double) im * im);
    };

    const double binHz = sr / fftSize;
    std::vector<bool> isHarmonic ((size_t) fftSize / 2, false);
    for (int k = 1; k * f0 < sr / 2.0; ++k)
    {
        const int centre = (int) std::round (k * f0 / binHz);
        for (int b = centre - 6; b <= centre + 6; ++b)
            if (b >= 0 && b < fftSize / 2)
                isHarmonic[(size_t) b] = true;
    }
    for (int b = 0; b < (int) (30.0 / binHz) + 1; ++b)
        isHarmonic[(size_t) b] = true;

    const double fundamental = binMag ((int) std::round (f0 / binHz));
    const int audibleLimitBin = (int) (20000.0 / binHz);

    double worst = 0.0;
    for (int b = 1; b <= audibleLimitBin; ++b)
        if (! isHarmonic[(size_t) b])
            worst = jmax (worst, binMag (b));

    const double db = Decibels::gainToDecibels (worst / jmax (1.0e-12, fundamental), -200.0);
    check (db < -60.0, "typical-material audible alias below -60 dB at default quality ("
                           + String (db, 1) + " dB)");
}

static void testColorSwitchAndLatency()
{
    section ("Phase 3: colour switch is click-free; latency reported per quality");

    const double sr = 48000.0;

    NonlinearStage stage;
    stage.prepare (sr, 512, 1);

    //  Phase 12 contract: the reported latency is the SAME for every quality.
    //  Each oversampler's own latency is padded up to the worst case.
    const float reported = stage.getLatencySamples();
    for (int q = 0; q < 4; ++q)
    {
        const float raw = stage.getRawLatencySamples ((Quality) q);
        std::printf ("      quality %dx: oversampler %.3f + pad %.3f = %.3f samples\n",
                     1 << q, raw, reported - raw, reported);
    }

    for (int q = 0; q < 4; ++q)
    {
        stage.setQuality ((Quality) q);
        check (stage.getLatencySamples() == reported,
               "quality " + String (1 << q) + "x reports the same latency ("
                   + String (stage.getLatencySamples(), 3) + ")");
    }

    //  Switch colour mid-playback. A hard-driven engine legitimately outputs
    //  near-square edges, so "no clicks" cannot mean "small sample steps";
    //  it means the steps during the fade windows are no larger than the
    //  steps the involved engines produce in steady state.
    stage.setQuality (Quality::high);
    stage.setColor (ColorType::warm);
    stage.setDrive (60.0f);
    stage.reset();

    double maxStepFade = 0.0, maxStepSteady = 0.0;
    float prev = 0.0f;
    int sampleIndex = 0;
    bool finite = true;

    //  Fade length is 15 ms = 720 samples at 48k = under 2 blocks of 512.
    auto isFadeBlock = [] (int blk) { return (blk >= 20 && blk <= 22) || (blk >= 40 && blk <= 42); };

    for (int blk = 0; blk < 60; ++blk)
    {
        if (blk == 20) stage.setColor (ColorType::fuzz);
        if (blk == 40) stage.setColor (ColorType::iron);

        AudioBuffer<float> buf (1, 512);
        auto* d = buf.getWritePointer (0);
        for (int i = 0; i < 512; ++i, ++sampleIndex)
            d[i] = 0.4f * (float) std::sin (2.0 * MathConstants<double>::pi * 330.0 * sampleIndex / sr);

        stage.process (buf);

        for (int i = 0; i < 512; ++i)
        {
            const float s = d[i];
            if (! std::isfinite (s)) finite = false;

            const double step = std::abs (s - prev);
            if (isFadeBlock (blk))
                maxStepFade = jmax (maxStepFade, step);
            else if (blk > 4)
                maxStepSteady = jmax (maxStepSteady, step);

            prev = s;
        }
    }

    check (finite, "colour switching output stays finite");
    check (maxStepFade <= maxStepSteady * 1.5 + 0.02,
           "colour switch adds no clicks beyond the engines' own waveform edges (fade max step "
               + String (maxStepFade, 4) + ", steady max step " + String (maxStepSteady, 4) + ")");
}

// ================================================================================
// Phase 4 — per-band processing chain
// ================================================================================
namespace
{
    //  A fully configured engine driven directly (no plugin wrapper).
    void runEngineBlocks (FourColorEngine& engine, const EngineParameters& p,
                          AudioBuffer<float>& io,
                          const std::function<float (int ch, int sampleIndex)>& gen,
                          int numBlocks, int block,
                          const std::function<void (int blk, AudioBuffer<float>&)>& inspect = {})
    {
        engine.setParameters (p);
        int sampleIndex = 0;

        for (int blk = 0; blk < numBlocks; ++blk)
        {
            for (int i = 0; i < block; ++i, ++sampleIndex)
                for (int c = 0; c < io.getNumChannels(); ++c)
                    io.setSample (c, i, gen (c, sampleIndex));

            engine.setParameters (p);
            engine.process (io);

            if (inspect)
                inspect (blk, io);
        }
    }
}

static void testCleanReconstruction()
{
    section ("Phase 4: all bands bypassed = flat clean reconstruction");

    //  With every band bypassed the output is the crossover's allpass of the
    //  input: flat magnitude at every probe frequency.
    const double sr = 48000.0;
    const int block = 512;

    FourColorEngine engine;
    engine.prepare (sr, block, 1);

    EngineParameters p;
    for (auto& b : p.bands)
        b.bypass = true;
    p.autoLevel = false;

    for (double freq : { 40.0, 120.0, 700.0, 4500.0, 12000.0 })
    {
        engine.reset();
        engine.setParameters (p);

        AudioBuffer<float> io (1, block);
        double sq = 0.0;
        int counted = 0;

        runEngineBlocks (engine, p, io,
            [&] (int, int s) { return 0.4f * (float) std::sin (2.0 * MathConstants<double>::pi * freq * s / sr); },
            80, block,
            [&] (int blk, AudioBuffer<float>& buf)
            {
                if (blk >= 40)
                {
                    for (int i = 0; i < block; ++i)
                        sq += (double) buf.getSample (0, i) * buf.getSample (0, i);
                    counted += block;
                }
            });

        const double outRms = std::sqrt (sq / counted);
        const double devDb = Decibels::gainToDecibels (outRms / (0.4 / std::sqrt (2.0)), -120.0);
        checkNear (devDb, 0.0, 0.05, "clean reconstruction flat at " + String (freq, 0) + " Hz");
    }
}

static void testSoloMuteLevel()
{
    section ("Phase 4: solo / mute / band level");

    const double sr = 48000.0;
    const int block = 512;

    auto bandRmsFor = [&] (const EngineParameters& p, double freq)
    {
        FourColorEngine engine;
        engine.prepare (sr, block, 1);
        engine.setParameters (p);

        AudioBuffer<float> io (1, block);
        double sq = 0.0; int counted = 0;
        int sampleIndex = 0;

        for (int blk = 0; blk < 60; ++blk)
        {
            for (int i = 0; i < block; ++i, ++sampleIndex)
                io.setSample (0, i, 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * freq * sampleIndex / sr));

            engine.setParameters (p);
            engine.process (io);

            if (blk >= 30)
            {
                for (int i = 0; i < block; ++i)
                    sq += (double) io.getSample (0, i) * io.getSample (0, i);
                counted += block;
            }
        }
        return std::sqrt (sq / counted);
    };

    EngineParameters base;
    for (auto& b : base.bands)
        b.bypass = true;      // keep engines out of the level comparison
    base.autoLevel = false;

    //  Solo the LOW band: 60 Hz passes, 8 kHz collapses.
    {
        auto p = base;
        p.bands[0].solo = true;
        const double low  = bandRmsFor (p, 60.0);
        const double high = bandRmsFor (p, 8000.0);
        const double refLow = bandRmsFor (base, 60.0);

        check (low > refLow * 0.7, "solo LOW keeps low content ("
                   + String (Decibels::gainToDecibels (low / refLow), 2) + " dB)");
        check (high < refLow * 0.02, "solo LOW rejects 8 kHz by >34 dB ("
                   + String (Decibels::gainToDecibels (high / refLow), 1) + " dB)");
    }

    //  Mute all bands: silence.
    {
        auto p = base;
        for (auto& b : p.bands)
            b.mute = true;
        const double rms = bandRmsFor (p, 300.0);
        check (rms < 1.0e-5, "all bands muted = silence (rms " + String (rms, 8) + ")");
    }

    //  Band level: -12 dB on the LOW band moves a 60 Hz sine by about -12 dB.
    //  (Bands must NOT be bypassed for level to apply; use drive 0 WARM.)
    {
        EngineParameters p;
        p.autoLevel = false;
        for (auto& b : p.bands)
            b.drive = 0.0f;

        auto pLow = p;
        pLow.bands[0].levelDb = -12.0f;

        //  Tolerance: at 60 Hz the LMID band still contributes -24 dB of
        //  leakage (LR4 slope one octave under the 120 Hz cut) which is not
        //  attenuated by band 0's level - the exact sum is about -10.3 dB.
        const double ref = bandRmsFor (p, 60.0);
        const double cut = bandRmsFor (pLow, 60.0);
        checkNear (Decibels::gainToDecibels (cut / ref), -12.0, 2.0,
                   "band 0 level -12 dB moves 60 Hz by ~-12 dB (LR4 leakage included)");
    }
}

static void testEngineMatrix()
{
    section ("Phase 4: sample-rate / block-size / channel matrix");

    //  Every configuration must produce finite audio at a sane level and must
    //  not allocate on the audio thread.
    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const int blocks[] = { 1, 16, 32, 64, 128, 512, 1024, 2048 };

    for (double sr : rates)
    {
        for (int block : blocks)
        {
            for (int chans : { 1, 2 })
            {
                FourColorEngine engine;
                engine.prepare (sr, block, chans);

                EngineParameters p;
                p.bands[0].color = ColorType::warm;
                p.bands[1].color = ColorType::iron;
                p.bands[2].color = ColorType::bite;
                p.bands[3].color = ColorType::fuzz;
                for (auto& b : p.bands)
                    b.drive = 60.0f;
                p.autoLevel = false;
                engine.setParameters (p);

                AudioBuffer<float> io (chans, block);
                bool finite = true;
                float peak = 0.0f;
                int sampleIndex = 0;

                const int numBlocks = jmax (4, 4096 / block);

                //  Warm up before arming the allocation tripwire.
                for (int blk = 0; blk < 2; ++blk)
                {
                    for (int i = 0; i < block; ++i, ++sampleIndex)
                        for (int c = 0; c < chans; ++c)
                            io.setSample (c, i, 0.35f * (float) std::sin (2.0 * MathConstants<double>::pi * 180.0 * sampleIndex / sr)
                                                + 0.15f * (float) std::sin (2.0 * MathConstants<double>::pi * 3000.0 * sampleIndex / sr));
                    engine.process (io);
                }

                allocationCount.store (0);
                trackAllocations.store (true);

                for (int blk = 0; blk < numBlocks; ++blk)
                {
                    for (int i = 0; i < block; ++i, ++sampleIndex)
                        for (int c = 0; c < chans; ++c)
                            io.setSample (c, i, 0.35f * (float) std::sin (2.0 * MathConstants<double>::pi * 180.0 * sampleIndex / sr)
                                                + 0.15f * (float) std::sin (2.0 * MathConstants<double>::pi * 3000.0 * sampleIndex / sr));

                    engine.setParameters (p);
                    engine.process (io);

                    for (int c = 0; c < chans; ++c)
                        for (int i = 0; i < block; ++i)
                        {
                            const float v = io.getSample (c, i);
                            if (! std::isfinite (v)) finite = false;
                            peak = jmax (peak, std::abs (v));
                        }
                }

                trackAllocations.store (false);
                const int allocs = allocationCount.load();

                const bool ok = finite && peak < 4.0f && allocs == 0;
                if (! ok || (block == 2048 && chans == 2))
                    check (ok, String (sr / 1000.0, 1) + " kHz / block " + String (block)
                                   + " / " + String (chans) + "ch: finite=" + String ((int) finite)
                                   + " peak=" + String (peak, 3) + " allocs=" + String (allocs));
                else
                    ++checksRun;   // count quiet passes without 80 lines of noise
            }
        }
    }

    std::printf ("      matrix: %d configurations checked\n", 5 * 8 * 2);
}

static void testQualitySwitchDuringPlayback()
{
    section ("Phase 4: quality switch during playback");

    const double sr = 48000.0;
    const int block = 512;

    FourColorEngine engine;
    engine.prepare (sr, block, 2);

    EngineParameters p;
    for (auto& b : p.bands)
        b.drive = 50.0f;
    p.autoLevel = false;

    AudioBuffer<float> io (2, block);
    bool finite = true;
    int sampleIndex = 0;
    int latencies[4] = {};

    for (int blk = 0; blk < 80; ++blk)
    {
        p.quality = (Quality) ((blk / 20) % 4);

        for (int i = 0; i < block; ++i, ++sampleIndex)
            for (int c = 0; c < 2; ++c)
                io.setSample (c, i, 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 300.0 * sampleIndex / sr));

        engine.setParameters (p);
        engine.process (io);
        latencies[(int) p.quality] = engine.getLatencySamples();

        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
                if (! std::isfinite (io.getSample (c, i)))
                    finite = false;
    }

    check (finite, "output stays finite through quality switches");

    //  Phase 12 replaced "latency grows with quality" with the opposite
    //  promise: it never moves, so a host is never handed a new number while
    //  the transport is running.
    check (latencies[0] == 65 && latencies[1] == 65 && latencies[2] == 65 && latencies[3] == 65,
           "latency is 65 samples in every quality (" + String (latencies[0]) + "/"
               + String (latencies[1]) + "/" + String (latencies[2]) + "/"
               + String (latencies[3]) + " smp)");
}

// ================================================================================
//  Phase 12: switching Quality during playback
// ================================================================================
static void testQualitySwitchIsSeamless()
{
    section ("Phase 12: Quality switches are click-free and time-invariant");

    const double sr = 48000.0;
    const int block = 128;

    //  Three programme types, because a discontinuity hides in different places
    //  in each: a tone shows steps, noise shows dropouts, a transient train
    //  shows smearing.
    enum class Material { sine, pinkish, transients };
    const char* materialNames[] = { "sine", "pink-ish noise", "transient train" };

    auto fill = [] (AudioBuffer<float>& buf, Material m, int startSample, Random& rng,
                    float& pinkState)
    {
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const int s = startSample + i;
            float v = 0.0f;

            switch (m)
            {
                case Material::sine:
                    v = 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.0 * s / 48000.0);
                    break;
                case Material::pinkish:
                    //  One-pole lowpassed white: broadband but not full-scale HF.
                    pinkState += 0.12f * (rng.nextFloat() * 2.0f - 1.0f - pinkState);
                    v = 0.9f * pinkState;
                    break;
                case Material::transients:
                {
                    const double phase = std::fmod ((double) s / 48000.0, 0.125);
                    v = 0.6f * (float) (std::exp (-phase * 40.0)
                                        * std::sin (2.0 * MathConstants<double>::pi * 90.0 * phase));
                    break;
                }
            }

            for (int c = 0; c < buf.getNumChannels(); ++c)
                buf.setSample (c, i, v);
        }
    };

    //  Envelope continuity, in dB between adjacent 32-sample windows. A click
    //  or a dropout shows up here whatever the material is, and the number is
    //  compared against the same measurement taken far from the switch, so the
    //  material's own liveliness cancels out. A raw "max sample step" budget
    //  only means something for smooth material - pink noise's own steps are
    //  0.3 FS, which is fifteen times the 0.02 FS budget.
    auto worstEnvelopeJumpDb = [] (const std::vector<float>& x, size_t first, size_t last)
    {
        constexpr size_t win = 32;
        double worst = 0.0, prev = -1.0;

        for (size_t w = first; w + win <= last; w += win)
        {
            double sumSq = 0.0;
            for (size_t i = 0; i < win; ++i)
                sumSq += (double) x[w + i] * x[w + i];
            const double rms = std::sqrt (sumSq / (double) win) + 1.0e-9;

            if (prev > 0.0)
                worst = jmax (worst, std::abs (20.0 * std::log10 (rms / prev)));
            prev = rms;
        }
        return worst;
    };

    const int switchBlock = 60, keptFromBlock = 20;
    const size_t switchSample = (size_t) ((switchBlock - keptFromBlock) * block);

    for (int m = 0; m < 3; ++m)
    {
        double worstStep = 0.0, baselineStep = 0.0, worstExcessDb = 0.0, worstPreSwitchDiff = 0.0;
        int longestSilentRun = 0;

        //  Every ordered pair of qualities.
        for (int from = 0; from < 4; ++from)
        {
            for (int to = 0; to < 4; ++to)
            {
                if (from == to)
                    continue;

                //  Two renders of the same input: one that switches, one that
                //  never does. Everything before the switch must be identical.
                std::vector<float> out[2];

                for (int variant = 0; variant < 2; ++variant)
                {
                    FourColorEngine engine;
                    engine.prepare (sr, block, 2);

                    EngineParameters p;
                    p.autoLevel = false;
                    p.quality = (Quality) from;
                    for (auto& b : p.bands)
                        b.drive = 45.0f;
                    engine.setParameters (p);

                    AudioBuffer<float> io (2, block);
                    Random rng (1234);
                    float pinkState = 0.0f;
                    int s = 0;

                    for (int blk = 0; blk < 140; ++blk)
                    {
                        if (variant == 1 && blk == switchBlock)
                        {
                            p.quality = (Quality) to;
                            engine.setParameters (p);
                        }

                        fill (io, (Material) m, s, rng, pinkState);
                        s += block;
                        engine.process (io);

                        if (blk >= keptFromBlock)
                            for (int i = 0; i < block; ++i)
                                out[variant].push_back (io.getSample (0, i));
                    }
                }

                //  1. Nothing before the switch may change.
                for (size_t i = 0; i < switchSample; ++i)
                    worstPreSwitchDiff = jmax (worstPreSwitchDiff,
                                               std::abs ((double) out[1][i] - out[0][i]));

                //  2. Envelope continuity across the switch, against the same
                //     measurement on quiet ground well before it.
                const double baselineDb = worstEnvelopeJumpDb (out[1], 0, switchSample - block);
                const double switchDb = worstEnvelopeJumpDb (out[1], switchSample - block,
                                                             switchSample + 4 * (size_t) block);
                worstExcessDb = jmax (worstExcessDb, switchDb - baselineDb);

                //  3. Raw step and dropout length, reported for every material
                //     and asserted only where the budget is meaningful.
                int silentRun = 0;
                for (size_t i = switchSample - (size_t) block;
                     i < switchSample + 4 * (size_t) block; ++i)
                {
                    worstStep = jmax (worstStep, std::abs ((double) out[1][i] - out[1][i - 1]));
                    if (std::abs (out[1][i]) < 1.0e-7f)
                    { ++silentRun; longestSilentRun = jmax (longestSilentRun, silentRun); }
                    else silentRun = 0;
                }

                //  The same measure taken well away from the switch, so the
                //  number above can be read as "signal" or "click".
                for (size_t i = (size_t) block + 1; i < switchSample - (size_t) block; ++i)
                    baselineStep = jmax (baselineStep, std::abs ((double) out[1][i] - out[1][i - 1]));
            }
        }

        std::printf ("      %-16s pre-switch diff %.9f, envelope excess %+.2f dB,"
                     " step %.5f FS (same material away from the switch: %.5f), silent run %d\n",
                     materialNames[m], worstPreSwitchDiff, worstExcessDb,
                     worstStep, baselineStep, longestSilentRun);

        check (worstPreSwitchDiff == 0.0,
               String (materialNames[m])
                   + ": a Quality change alters nothing before it happens (diff "
                   + String (worstPreSwitchDiff, 9) + ")");

        check (worstExcessDb < 1.0,
               String (materialNames[m])
                   + ": the switch adds no envelope discontinuity beyond the material's own ("
                   + String (worstExcessDb, 2) + " dB)");

        check (longestSilentRun <= 1,
               String (materialNames[m]) + ": no dropout longer than one sample ("
                   + String (longestSilentRun) + ")");

        //  The spec's 0.02 FS budget, applied where a sample step is a
        //  meaningful measure at all.
        if (m != 1)
            check (worstStep < 0.02,
                   String (materialNames[m]) + ": no switch steps more than 0.02 FS ("
                       + String (worstStep, 5) + ")");
    }
}

// ================================================================================
// Phase 5 — Behavior (BODY <-> ATTACK)
// ================================================================================
namespace
{
    //  A synthetic kick: pitch drop 120 -> 50 Hz, exponential amplitude decay,
    //  one hit per 250 ms.
    float kickSample (int i, double sr)
    {
        const int period = (int) (0.25 * sr);
        const int t = i % period;
        const double sec = t / sr;
        const double freq = 50.0 + 70.0 * std::exp (-sec * 30.0);
        const double phase = 2.0 * MathConstants<double>::pi * (50.0 * sec + (70.0 / 30.0) * (1.0 - std::exp (-sec * 30.0)));
        juce::ignoreUnused (freq);
        return (float) (0.8 * std::exp (-sec * 12.0) * std::sin (phase));
    }

    struct AttackBodyMeasure
    {
        double attack;        // peak level of the attack window
        double body;          // RMS of the body window
        double attackCrunch;  // derivative-energy ratio of the attack window:
                              // how much harmonic "edge" the hit carries
    };

    //  Run a kick train through one band processor and measure the attack
    //  window (0-15 ms) and body window (40-150 ms) of each hit. Driving a
    //  transient harder into a saturator COMPRESSES its peak, so ATTACK is
    //  audible as crunch (upper harmonics on the hit), not as extra level -
    //  hence the derivative-energy measure.
    AttackBodyMeasure measureKickTrain (float behaviorAmount)
    {
        const double sr = 48000.0;
        const int block = 512;

        BandProcessor band;
        band.prepare (sr, block, 1, 0);
        band.setQuality (Quality::high);

        BandProcessor::Settings s;
        s.color = ColorType::warm;
        s.drivePercent = 70.0f;
        s.behavior = behaviorAmount;
        s.centreHz = 60.0f;
        band.setSettings (s);

        const int period = (int) (0.25 * sr);
        const int total = period * 8;
        const int latency = (int) std::round (band.getLatencySamples());

        std::vector<float> out;
        out.reserve ((size_t) total);

        AudioBuffer<float> buf (1, block);
        for (int start = 0; start < total; start += block)
        {
            auto* d = buf.getWritePointer (0);
            for (int i = 0; i < block; ++i)
                d[i] = kickSample (start + i, sr);

            band.setSettings (s);
            band.process (buf);

            for (int i = 0; i < block; ++i)
                out.push_back (buf.getSample (0, i));
        }

        //  Skip the first two hits (detector settling), align by latency.
        AttackBodyMeasure m { 0.0, 0.0, 0.0 };
        int hits = 0;
        for (int h = 2; h < 8; ++h)
        {
            const int hitStart = h * period + latency;
            double atk = 0.0, body = 0.0, diffSq = 0.0, sq = 0.0;
            const int atkEnd = (int) (0.015 * sr), bodyStart = (int) (0.040 * sr), bodyEnd = (int) (0.150 * sr);

            for (int i = 0; i < atkEnd; ++i)
            {
                const double v = out[(size_t) (hitStart + i)];
                atk = jmax (atk, std::abs (v));
                sq += v * v;
                if (i > 0)
                {
                    const double d = v - out[(size_t) (hitStart + i - 1)];
                    diffSq += d * d;
                }
            }
            for (int i = bodyStart; i < bodyEnd; ++i)
                body += (double) out[(size_t) (hitStart + i)] * out[(size_t) (hitStart + i)];

            m.attack       += atk;
            m.body         += std::sqrt (body / (bodyEnd - bodyStart));
            m.attackCrunch += std::sqrt (diffSq / jmax (1.0e-12, sq));
            ++hits;
        }

        m.attack       /= hits;
        m.body         /= hits;
        m.attackCrunch /= hits;
        return m;
    }
}

static void testBehaviorAttackVsBody()
{
    section ("Phase 5: BODY vs ATTACK are different behaviours");

    const auto body    = measureKickTrain (-1.0f);
    const auto neutral = measureKickTrain (0.0f);
    const auto attack  = measureKickTrain (+1.0f);

    std::printf ("      attack crunch: BODY %.4f, neutral %.4f, ATTACK %.4f\n",
                 body.attackCrunch, neutral.attackCrunch, attack.attackCrunch);
    std::printf ("      body RMS:      BODY %.4f, neutral %.4f, ATTACK %.4f\n",
                 body.body, neutral.body, attack.body);

    //  ATTACK distorts the hit harder than BODY: clearly more edge (derivative
    //  energy) in the attack window, monotone across the range.
    const double crunchGapDb = Decibels::gainToDecibels (attack.attackCrunch / body.attackCrunch);
    check (crunchGapDb > 1.5, "ATTACK puts >1.5 dB more crunch on the hit than BODY ("
                                  + String (crunchGapDb, 2) + " dB)");
    check (attack.attackCrunch >= neutral.attackCrunch * 0.98
               && neutral.attackCrunch >= body.attackCrunch * 0.98,
           "crunch is monotone from BODY through neutral to ATTACK");

    //  And it must NOT be a volume knob: overall body level within 3 dB.
    const double bodyShift = std::abs (Decibels::gainToDecibels (attack.body / body.body));
    check (bodyShift < 3.0, "sustain level shift between extremes is bounded ("
                                + String (bodyShift, 2) + " dB)");
}

static void testBehaviorNoPumpingOnSustained()
{
    section ("Phase 5: no pumping on sustained material");

    //  On a steady sine the transient measure decays to ~0, so Behavior at
    //  either extreme must barely change the steady-state level.
    const double sr = 48000.0;
    const int block = 512;

    auto steadyRms = [&] (float behaviorAmount)
    {
        BandProcessor band;
        band.prepare (sr, block, 1, 1);
        band.setQuality (Quality::high);

        BandProcessor::Settings s;
        s.color = ColorType::iron;
        s.drivePercent = 60.0f;
        s.behavior = behaviorAmount;
        s.centreHz = 400.0f;
        band.setSettings (s);

        AudioBuffer<float> buf (1, block);
        double sq = 0.0; int counted = 0;
        int sampleIndex = 0;

        for (int blk = 0; blk < 120; ++blk)
        {
            auto* d = buf.getWritePointer (0);
            for (int i = 0; i < block; ++i, ++sampleIndex)
                d[i] = 0.35f * (float) std::sin (2.0 * MathConstants<double>::pi * 400.0 * sampleIndex / sr);

            band.setSettings (s);
            band.process (buf);

            if (blk >= 60)
            {
                for (int i = 0; i < block; ++i)
                    sq += (double) buf.getSample (0, i) * buf.getSample (0, i);
                counted += block;
            }
        }
        return std::sqrt (sq / counted);
    };

    const double neutral = steadyRms (0.0f);
    const double bodyDev = std::abs (Decibels::gainToDecibels (steadyRms (-1.0f) / neutral));
    const double atkDev  = std::abs (Decibels::gainToDecibels (steadyRms (+1.0f) / neutral));

    check (bodyDev < 1.0, "BODY extreme shifts sustained level < 1 dB ("
                              + String (bodyDev, 2) + " dB)");
    check (atkDev < 1.0, "ATTACK extreme shifts sustained level < 1 dB ("
                             + String (atkDev, 2) + " dB)");
}

static void testBehaviorStereoLinked()
{
    section ("Phase 5: behavior detector is stereo-linked");

    //  A transient only in the LEFT channel must modulate both channels with
    //  the same curve: the R/L gain ratio for a shared steady component stays
    //  constant through the hit.
    const double sr = 48000.0;
    const int block = 512;

    BandProcessor band;
    band.prepare (sr, block, 2, 2);
    band.setQuality (Quality::high);

    BandProcessor::Settings s;
    s.color = ColorType::warm;
    s.drivePercent = 55.0f;
    s.behavior = 1.0f;
    s.centreHz = 1300.0f;
    band.setSettings (s);

    //  Both channels carry the same quiet 1 kHz tone; L additionally gets a
    //  burst every 250 ms. If the detector were per-channel, L's tone would be
    //  driven differently from R's during the burst.
    AudioBuffer<float> buf (2, block);
    std::vector<float> outL, outR;
    int sampleIndex = 0;

    for (int blk = 0; blk < 100; ++blk)
    {
        for (int i = 0; i < block; ++i, ++sampleIndex)
        {
            const float tone = 0.1f * (float) std::sin (2.0 * MathConstants<double>::pi * 1000.0 * sampleIndex / sr);
            const int t = sampleIndex % (int) (0.25 * sr);
            const float burst = t < (int) (0.01 * sr)
                ? 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * 2000.0 * t / sr)
                : 0.0f;
            buf.setSample (0, i, tone + burst);
            buf.setSample (1, i, tone);
        }

        band.setSettings (s);
        band.process (buf);

        for (int i = 0; i < block; ++i)
        {
            outL.push_back (buf.getSample (0, i));
            outR.push_back (buf.getSample (1, i));
        }
    }

    //  Measure R's steady tone level in the 30 ms after each burst versus far
    //  from bursts; with a linked detector both see the same modulation, so R
    //  is modulated NEAR the burst even though R itself has no burst.
    const int period = (int) (0.25 * sr);
    double nearBurst = 0.0, farBurst = 0.0;
    int nearCount = 0, farCount = 0;

    for (size_t i = (size_t) period * 2; i < outR.size(); ++i)
    {
        const int t = (int) (i % (size_t) period);
        const double v = (double) outR[i] * outR[i];
        if (t > (int) (0.001 * sr) && t < (int) (0.010 * sr)) { nearBurst += v; ++nearCount; }
        if (t > (int) (0.100 * sr) && t < (int) (0.200 * sr)) { farBurst += v; ++farCount; }
    }

    const double modDepthDb = Decibels::gainToDecibels (
        std::sqrt (nearBurst / nearCount) / std::sqrt (farBurst / farCount));

    std::printf ("      R-channel modulation near L-only burst: %.2f dB\n", modDepthDb);
    check (modDepthDb > 1.0, "linked detector modulates R for an L-only transient ("
                                 + String (modDepthDb, 2) + " dB)");
}

// ================================================================================
// Phase 6 — Harmonic Space
// ================================================================================
namespace
{
    //  Renders a band through BandProcessor and returns the full output.
    std::vector<std::vector<float>> renderBand (int bandIndex, int chans, float drive,
                                                float spacePercent, double sr, int totalSamples,
                                                const std::function<float (int c, int s)>& gen)
    {
        const int block = 512;

        BandProcessor band;
        band.prepare (sr, block, chans, bandIndex);
        band.setQuality (Quality::high);

        BandProcessor::Settings s;
        s.color = ColorType::iron;
        s.drivePercent = drive;
        s.spacePercent = spacePercent;
        s.centreHz = 400.0f;
        band.setSettings (s);

        std::vector<std::vector<float>> out ((size_t) chans);
        AudioBuffer<float> buf (chans, block);

        for (int start = 0; start < totalSamples; start += block)
        {
            for (int i = 0; i < block; ++i)
                for (int c = 0; c < chans; ++c)
                    buf.setSample (c, i, gen (c, start + i));

            band.setSettings (s);
            band.process (buf);

            for (int i = 0; i < block; ++i)
                for (int c = 0; c < chans; ++c)
                    out[(size_t) c].push_back (buf.getSample (c, i));
        }
        return out;
    }

    double diffRms (const std::vector<float>& a, const std::vector<float>& b, size_t from)
    {
        double sq = 0.0;
        for (size_t i = from; i < a.size(); ++i)
            sq += (double) (a[i] - b[i]) * (a[i] - b[i]);
        return std::sqrt (sq / (a.size() - from));
    }
}

static void testSpaceIsHarmonicOnly()
{
    section ("Phase 6: Space works on the nonlinear residual only");

    const double sr = 48000.0;
    const int total = 48000 * 2;

    //  Two tones so the saturation makes intermodulation as well. The QUIET
    //  variant drives the same curve in its near-linear region: if Space were
    //  a reverb on the source, its relative halo would be level-independent;
    //  because it diffuses the RESIDUAL, the relative halo must collapse with
    //  the nonlinearity. (Note the engines colour even at drive 0 by design,
    //  so "drive 0 = clean" is not a valid reference - level is.)
    auto genAt = [sr] (float scale)
    {
        return [sr, scale] (int, int s)
        {
            return scale * (0.30f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.0 * s / sr)
                          + 0.15f * (float) std::sin (2.0 * MathConstants<double>::pi * 470.0 * s / sr));
        };
    };

    const auto quietOff = renderBand (1, 1, 80.0f, 0.0f, sr, total, genAt (0.1f));
    const auto quietOn  = renderBand (1, 1, 80.0f, 100.0f, sr, total, genAt (0.1f));
    const auto hotOff   = renderBand (1, 1, 80.0f, 0.0f, sr, total, genAt (1.0f));
    const auto hotOn    = renderBand (1, 1, 80.0f, 100.0f, sr, total, genAt (1.0f));

    const size_t settle = 48000;   // let the least-squares fit converge
    auto rmsFrom = [] (const std::vector<float>& v, size_t from)
    {
        double sq = 0.0;
        for (size_t i = from; i < v.size(); ++i) sq += (double) v[i] * v[i];
        return std::sqrt (sq / (v.size() - from));
    };

    const double haloHotRel   = diffRms (hotOn[0], hotOff[0], settle)     / rmsFrom (hotOff[0], settle);
    const double haloQuietRel = diffRms (quietOn[0], quietOff[0], settle) / rmsFrom (quietOff[0], settle);

    std::printf ("      relative halo: quiet (near-linear) %.5f, hot (saturated) %.5f\n",
                 haloQuietRel, haloHotRel);

    check (haloHotRel > 0.05, "space adds an audible halo when the engine distorts (rel rms "
                                  + String (haloHotRel, 4) + ")");
    check (haloHotRel > 4.0 * haloQuietRel,
           "the halo follows the DISTORTION, not the source (hot/quiet relative ratio "
               + String (haloHotRel / jmax (1.0e-9, haloQuietRel), 1) + "x)");
}

static void testSpaceMonoBassAndCorrelation()
{
    section ("Phase 6: LOW space stays mono; upper space keeps mono compatibility");

    const double sr = 48000.0;
    const int total = 48000 * 2;

    //  LOW band (index 0), stereo in: the ADDED halo must be identical L/R.
    {
        auto gen = [sr] (int, int s)
        { return 0.4f * (float) std::sin (2.0 * MathConstants<double>::pi * 70.0 * s / sr); };

        const auto off = renderBand (0, 2, 80.0f, 0.0f, sr, total, gen);
        const auto on  = renderBand (0, 2, 80.0f, 100.0f, sr, total, gen);

        double maxLrDiff = 0.0;
        for (size_t i = 48000; i < on[0].size(); ++i)
        {
            const double haloL = on[0][i] - off[0][i];
            const double haloR = on[1][i] - off[1][i];
            maxLrDiff = jmax (maxLrDiff, std::abs (haloL - haloR));
        }
        check (maxLrDiff < 1.0e-6, "LOW-band halo is strictly mono (max L/R diff "
                                       + String (maxLrDiff, 9) + ")");
    }

    //  HMID band, stereo: halo is decorrelated but the total stays mono-safe
    //  (positive correlation - no phase cancellation on fold-down).
    {
        auto gen = [sr] (int, int s)
        { return 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 1300.0 * s / sr); };

        const auto on = renderBand (2, 2, 80.0f, 100.0f, sr, total, gen);

        double lr = 0.0, ll = 0.0, rr = 0.0;
        for (size_t i = 48000; i < on[0].size(); ++i)
        {
            lr += (double) on[0][i] * on[1][i];
            ll += (double) on[0][i] * on[0][i];
            rr += (double) on[1][i] * on[1][i];
        }
        const double correlation = lr / std::sqrt (jmax (1.0e-18, ll * rr));
        check (correlation > 0.5, "HMID at full space keeps L/R correlation > 0.5 ("
                                      + String (correlation, 3) + ")");

        //  And the channels are actually decorrelated (it IS stereo).
        check (correlation < 0.999, "upper-band space is not just dual mono ("
                                        + String (correlation, 4) + ")");
    }
}

static void testSpaceDecaysAndZeroCost()
{
    section ("Phase 6: Space decays fast and is skipped at 0%");

    const double sr = 48000.0;

    //  A burst then silence: the halo must die quickly (no reverb tail).
    {
        auto gen = [sr] (int, int s)
        {
            return s < 4800 ? 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * 500.0 * s / sr)
                            : 0.0f;
        };

        const auto on = renderBand (1, 1, 80.0f, 100.0f, sr, 48000, gen);

        //  Level 300 ms after the burst ends.
        double post = 0.0;
        const size_t from = 4800 + (size_t) (0.3 * sr);
        for (size_t i = from; i < from + 4800; ++i)
            post = jmax (post, (double) std::abs (on[0][i]));

        check (post < 1.0e-4, "halo fully decayed 300 ms after the source stopped (peak "
                                  + String (post, 7) + ")");
    }

    //  Returning to 0% leaves no residue: a run that had space at 100% and
    //  then turned it off converges to the same steady output as a run where
    //  space was never on (the skip path really is clean).
    {
        const int block = 512;
        auto makeBand = [&] { auto b = std::make_unique<BandProcessor>();
                              b->prepare (sr, block, 1, 1);
                              b->setQuality (Quality::high);
                              return b; };
        auto bandA = makeBand(), bandB = makeBand();

        BandProcessor::Settings sA, sB;
        sA.drivePercent = sB.drivePercent = 70.0f;
        sA.spacePercent = 0.0f;

        AudioBuffer<float> bufA (1, block), bufB (1, block);
        double maxTailDiff = 0.0;
        int sampleIndex = 0;

        for (int blk = 0; blk < 200; ++blk)
        {
            sB.spacePercent = blk < 60 ? 100.0f : 0.0f;   // on, then off

            for (int i = 0; i < block; ++i, ++sampleIndex)
            {
                const float v = 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 300.0 * sampleIndex / sr);
                bufA.setSample (0, i, v);
                bufB.setSample (0, i, v);
            }

            bandA->setSettings (sA); bandA->process (bufA);
            bandB->setSettings (sB); bandB->process (bufB);

            if (blk >= 150)   // well after the off-ramp and comb drain
                for (int i = 0; i < block; ++i)
                    maxTailDiff = jmax (maxTailDiff, (double) std::abs (
                        bufA.getSample (0, i) - bufB.getSample (0, i)));
        }

        check (maxTailDiff < 1.0e-6, "space off leaves no residue vs never-on ("
                                         + String (maxTailDiff, 9) + ")");
    }
}

// ================================================================================
// Phase 7 — global integration
// ================================================================================
namespace
{
    //  Steady output RMS of the full engine for a generator, after settling.
    double engineSteadyRms (const EngineParameters& p, double freq, double sr = 48000.0,
                            float amp = 0.3f, int settleBlocks = 60, int measureBlocks = 60)
    {
        const int block = 512;
        FourColorEngine engine;
        engine.prepare (sr, block, 1);
        engine.setParameters (p);

        AudioBuffer<float> io (1, block);
        double sq = 0.0; int counted = 0;
        int sampleIndex = 0;

        for (int blk = 0; blk < settleBlocks + measureBlocks; ++blk)
        {
            auto* d = io.getWritePointer (0);
            for (int i = 0; i < block; ++i, ++sampleIndex)
                d[i] = amp * (float) std::sin (2.0 * MathConstants<double>::pi * freq * sampleIndex / sr);

            engine.setParameters (p);
            engine.process (io);

            if (blk >= settleBlocks)
            {
                for (int i = 0; i < block; ++i)
                    sq += (double) io.getSample (0, i) * io.getSample (0, i);
                counted += block;
            }
        }
        return std::sqrt (sq / counted);
    }
}

// ================================================================================
//  Phase 11: Drive 0 is a clean pass-through
// ================================================================================
namespace
{
    const char* const engineNames[] = { "WARM", "IRON", "BITE", "FUZZ" };

    //  Renders `seconds` of a sine through a fully configured engine and hands
    //  back the settled tail.
    std::vector<float> renderSine (const EngineParameters& p, double freq,
                                   float amplitude, double sr, int block,
                                   int settleSamples, int measureSamples)
    {
        FourColorEngine engine;
        engine.prepare (sr, block, 1);
        engine.setParameters (p);

        std::vector<float> out;
        out.reserve ((size_t) (settleSamples + measureSamples));

        AudioBuffer<float> io (1, block);
        int s = 0;
        while ((int) out.size() < settleSamples + measureSamples)
        {
            auto* d = io.getWritePointer (0);
            for (int i = 0; i < block; ++i, ++s)
                d[i] = amplitude * (float) std::sin (2.0 * MathConstants<double>::pi * freq * s / sr);

            engine.setParameters (p);
            engine.process (io);

            for (int i = 0; i < block; ++i)
                out.push_back (io.getSample (0, i));
        }

        out.erase (out.begin(), out.begin() + settleSamples);
        out.resize ((size_t) measureSamples);
        return out;
    }

    //  THD+N of a single-tone render: everything that is not the fundamental,
    //  as dB relative to the total. The fundamental is removed by subtracting
    //  its least-squares sine/cosine projection, which needs no window.
    double thdPlusNoiseDb (const std::vector<float>& x, double freq, double sr)
    {
        const auto n = (double) x.size();
        double sumSin = 0.0, sumCos = 0.0;

        for (size_t i = 0; i < x.size(); ++i)
        {
            const double w = 2.0 * MathConstants<double>::pi * freq * (double) i / sr;
            sumSin += x[i] * std::sin (w);
            sumCos += x[i] * std::cos (w);
        }

        const double a = 2.0 * sumSin / n, b = 2.0 * sumCos / n;

        double totalSq = 0.0, residualSq = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double w = 2.0 * MathConstants<double>::pi * freq * (double) i / sr;
            const double fundamental = a * std::sin (w) + b * std::cos (w);
            const double r = x[i] - fundamental;
            totalSq    += (double) x[i] * x[i];
            residualSq += r * r;
        }

        if (totalSq <= 0.0)
            return -200.0;

        return 10.0 * std::log10 (jmax (1.0e-30, residualSq / totalSq));
    }
}

static void testDriveZeroIsClean()
{
    section ("Phase 11: Drive 0 is a clean pass-through in every engine and band");

    const double sr = 48000.0;
    const int block = 512;

    //  One probe frequency well inside each band, with the default crossovers
    //  at 120 Hz / 700 Hz / 4.5 kHz.
    struct Probe { const char* band; double freq; };
    const Probe probes[] = { { "LOW", 60.0 }, { "LOW MID", 300.0 },
                             { "HIGH MID", 1500.0 }, { "HIGH", 8000.0 } };
    double worstThd = -200.0, worstGainDb = 0.0;

    for (int colorIndex = 0; colorIndex < 4; ++colorIndex)
    {
        for (const auto& probe : probes)
        {
            EngineParameters p;
            p.autoLevel = false;
            for (auto& b : p.bands)
            {
                b.color = (ColorType) colorIndex;
                b.drive = 0.0f;       // the contract under test
                b.space = 0.0f;
                b.behavior = 0.0f;
                b.tone = 0.0f;
            }

            constexpr float amplitude = 0.25f;
            const auto out = renderSine (p, probe.freq, amplitude, sr, block, 24000, 48000);

            const double thd = thdPlusNoiseDb (out, probe.freq, sr);
            worstThd = jmax (worstThd, thd);

            //  Level against the clean path. The four bands recombine to an
            //  allpass, so magnitude - not the waveform - is the thing to
            //  compare; a sine's RMS is amplitude/sqrt(2).
            double sumSq = 0.0;
            for (auto v : out) sumSq += (double) v * v;
            const double rms = std::sqrt (sumSq / (double) out.size());
            const double gainDb = Decibels::gainToDecibels (rms / (amplitude / std::sqrt (2.0)));
            worstGainDb = jmax (worstGainDb, std::abs (gainDb));

            if (std::abs (gainDb) > 0.05 || thd > -90.0)
                std::printf ("      %-4s %-8s @ %6.0f Hz: THD+N %7.1f dB, gain %+.4f dB\n",
                             engineNames[colorIndex], probe.band, probe.freq, thd, gainDb);
        }
    }

    std::printf ("      worst of 16 engine/band pairs: THD+N %.1f dB, |gain| %.4f dB\n",
                 worstThd, worstGainDb);

    check (worstThd < -90.0,
           "Drive 0 leaves no nonlinear residual in any engine or band (worst THD+N "
               + String (worstThd, 1) + " dB)");
    check (worstGainDb < 0.05,
           "Drive 0 is unity gain in any engine or band (worst |gain| "
               + String (worstGainDb, 4) + " dB)");
}

static void testDriveSweepThroughZero()
{
    section ("Phase 11: sweeping Drive through the engage window does not step");

    const double sr = 48000.0;
    const int block = 64;          // small blocks: worst case for a block-rate step
    const double freq = 110.0;
    constexpr float amplitude = 0.3f;

    //  A 110 Hz sine moves at most 0.3 * 2*pi*110/48000 = 0.0043 per sample, so
    //  anything approaching the 0.02 FS budget is a discontinuity, not signal.
    const double naturalSlope = amplitude * 2.0 * MathConstants<double>::pi * freq / sr;

    for (int colorIndex = 0; colorIndex < 4; ++colorIndex)
    {
        FourColorEngine engine;
        engine.prepare (sr, block, 1);

        EngineParameters p;
        p.autoLevel = false;
        for (auto& b : p.bands)
        {
            b.color = (ColorType) colorIndex;
            b.drive = 0.0f;
            b.space = 0.0f;
        }
        engine.setParameters (p);

        AudioBuffer<float> io (1, block);
        std::vector<float> out;
        const int totalBlocks = 1400;          // ~1.9 s: settle, 0->10, 10->0
        int s = 0;

        for (int blk = 0; blk < totalBlocks; ++blk)
        {
            //  Hold at 0, ramp 0->10 over ~0.35 s, hold, ramp back to 0.
            const double t = (double) blk / totalBlocks;
            float drive = 0.0f;
            if (t > 0.25 && t <= 0.45) drive = (float) ((t - 0.25) / 0.20) * 10.0f;
            else if (t > 0.45 && t <= 0.65) drive = 10.0f;
            else if (t > 0.65 && t <= 0.85) drive = 10.0f - (float) ((t - 0.65) / 0.20) * 10.0f;

            for (auto& b : p.bands)
                b.drive = drive;
            engine.setParameters (p);

            auto* d = io.getWritePointer (0);
            for (int i = 0; i < block; ++i, ++s)
                d[i] = amplitude * (float) std::sin (2.0 * MathConstants<double>::pi * freq * s / sr);

            engine.process (io);

            //  Skip the first 0.1 s: the engine's own startup is not the subject.
            if (blk > 80)
                for (int i = 0; i < block; ++i)
                    out.push_back (io.getSample (0, i));
        }

        double worstStep = 0.0;
        for (size_t i = 1; i < out.size(); ++i)
            worstStep = jmax (worstStep, std::abs ((double) out[i] - out[i - 1]));

        std::printf ("      %-4s: worst sample step %.5f FS (signal's own slope %.5f)\n",
                     engineNames[colorIndex], worstStep, naturalSlope);

        check (worstStep < 0.02,
               String (engineNames[colorIndex])
                   + ": Drive 0->10->0 sweep steps no more than 0.02 FS ("
                   + String (worstStep, 5) + ")");
    }
}

static void testGlobalDrive()
{
    section ("Phase 7: Global Drive scales the bands' relative drives");

    //  Measure 3rd-harmonic content of a 220 Hz sine as globalDrive moves.
    const double sr = 48000.0;
    const int block = 512;

    auto renderWith = [&] (float globalDrive, float bandDrive, std::vector<float>& out)
    {
        FourColorEngine engine;
        engine.prepare (sr, block, 1);

        EngineParameters p;
        p.autoLevel = false;
        p.globalDrive = globalDrive;
        for (auto& b : p.bands)
            b.drive = bandDrive;
        engine.setParameters (p);

        const int settle = 48000, measure = 48000;
        out.clear();
        out.reserve ((size_t) (settle + measure));

        AudioBuffer<float> io (1, block);
        int sampleIndex = 0;
        while ((int) out.size() < settle + measure)
        {
            auto* d = io.getWritePointer (0);
            for (int i = 0; i < block; ++i, ++sampleIndex)
                d[i] = 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.5 * sampleIndex / sr);

            engine.setParameters (p);
            engine.process (io);

            for (int i = 0; i < block; ++i)
                out.push_back (io.getSample (0, i));
        }
    };

    auto h3For = [&] (float globalDrive)
    {
        FourColorEngine engine;
        engine.prepare (sr, block, 1);

        EngineParameters p;
        p.autoLevel = false;
        p.globalDrive = globalDrive;
        for (auto& b : p.bands)
            b.drive = 50.0f;
        engine.setParameters (p);

        const int settle = 48000, measure = 48000;
        std::vector<float> out;
        out.reserve ((size_t) (settle + measure));

        AudioBuffer<float> io (1, block);
        int sampleIndex = 0;
        while ((int) out.size() < settle + measure)
        {
            auto* d = io.getWritePointer (0);
            for (int i = 0; i < block; ++i, ++sampleIndex)
                d[i] = 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.5 * sampleIndex / sr);

            engine.setParameters (p);
            engine.process (io);

            for (int i = 0; i < block; ++i)
                out.push_back (io.getSample (0, i));
        }

        //  Third harmonic RELATIVE to the fundamental. Measuring the absolute
        //  h3 amplitude confounds "how much does it distort" with "how much
        //  make-up gain does the engine apply", so improving the static gain
        //  compensation would shrink the number without the drive scaling
        //  having changed at all. The ratio is make-up-gain invariant.
        const double h3 = goertzel (out.data() + settle, measure, 220.5 * 3.0, sr);
        const double h1 = goertzel (out.data() + settle, measure, 220.5, sr);
        return h3 / jmax (1.0e-12, h1);
    };

    const double lo = h3For (10.0f), mid = h3For (50.0f), hi = h3For (100.0f);
    std::printf ("      h3/h1: globalDrive 10 -> %.6f, 50 -> %.6f, 100 -> %.6f\n", lo, mid, hi);

    //  The qualitative claim: more Global Drive, more relative distortion.
    check (mid > lo * 1.5 && hi > mid * 1.5,
           "relative distortion rises with Global Drive ("
               + String (mid / jmax (1.0e-12, lo), 2) + "x then "
               + String (hi / jmax (1.0e-12, mid), 2) + "x)");

    //  The exact claim, and the one the parameter actually promises: Global
    //  Drive is a scale factor on the band drives, neutral at 50. So driving
    //  the bands at 20 with Global 50 must be the SAME SIGNAL as driving them
    //  at 50 with Global 20. This replaces a ">2x" threshold that had been
    //  passing by 0.25% and would move whenever the make-up gain changed.
    {
        std::vector<float> viaGlobal, viaBand;
        renderWith (20.0f, 50.0f, viaGlobal);
        renderWith (50.0f, 20.0f, viaBand);

        double worst = 0.0;
        const auto count = jmin (viaGlobal.size(), viaBand.size());
        for (size_t i = 0; i < count; ++i)
            worst = jmax (worst, std::abs ((double) viaGlobal[i] - viaBand[i]));

        check (worst < 1.0e-6,
               "Global Drive 20 x band 50 == Global 50 x band 20, sample for sample (worst "
                   + String (worst, 9) + ")");
    }
    check (hi > 1.5 * mid, "globalDrive 100 distorts clearly more than 50 ("
                               + String (hi / jmax (1.0e-12, mid), 1) + "x)");
}

static void testGlobalTone()
{
    section ("Phase 7: Global Tone tilts around 800 Hz");

    EngineParameters neutral;
    neutral.autoLevel = false;
    for (auto& b : neutral.bands)
        b.bypass = true;

    auto bright = neutral; bright.globalTone = 100.0f;
    auto dark   = neutral; dark.globalTone   = -100.0f;

    const double loN = engineSteadyRms (neutral, 200.0), hiN = engineSteadyRms (neutral, 4000.0);
    const double loB = engineSteadyRms (bright, 200.0),  hiB = engineSteadyRms (bright, 4000.0);
    const double loD = engineSteadyRms (dark, 200.0),    hiD = engineSteadyRms (dark, 4000.0);

    const double tiltB = Decibels::gainToDecibels ((hiB / hiN) / (loB / loN));
    const double tiltD = Decibels::gainToDecibels ((hiD / hiN) / (loD / loN));

    std::printf ("      bright tilt %.2f dB, dark tilt %.2f dB (4 kHz vs 200 Hz)\n", tiltB, tiltD);

    check (tiltB > 8.0, "BRIGHT tilts 4 kHz up vs 200 Hz by >8 dB (" + String (tiltB, 1) + ")");
    check (tiltD < -8.0, "DARK tilts 4 kHz down vs 200 Hz by >8 dB (" + String (tiltD, 1) + ")");
}

static void testAutoLevel()
{
    section ("Phase 7: Auto Level");

    //  Heavy FUZZ drive changes loudness; Auto Level must pull the output back
    //  towards the input level, slowly and within bounds.
    EngineParameters p;
    for (auto& b : p.bands)
    {
        b.color = ColorType::fuzz;
        b.drive = 95.0f;
    }

    p.autoLevel = false;
    const double without = engineSteadyRms (p, 220.0, 48000.0, 0.3f, 200, 100);
    p.autoLevel = true;
    const double with_   = engineSteadyRms (p, 220.0, 48000.0, 0.3f, 400, 100);

    const double inRms = 0.3 / std::sqrt (2.0);
    const double devWithout = std::abs (Decibels::gainToDecibels (without / inRms));
    const double devWith    = std::abs (Decibels::gainToDecibels (with_ / inRms));

    std::printf ("      deviation from input level: AL off %.2f dB, AL on %.2f dB\n",
                 devWithout, devWith);

    check (devWith < devWithout, "Auto Level reduces the loudness error");
    check (devWith < 1.5, "Auto Level lands within 1.5 dB of the input level ("
                              + String (devWith, 2) + " dB)");

    //  Silence handling: after the signal stops, the gain must hold, not drift.
    {
        const int block = 512;
        FourColorEngine engine;
        engine.prepare (48000.0, block, 1);
        engine.setParameters (p);

        AudioBuffer<float> io (1, block);
        int sampleIndex = 0;

        //  2 s of tone, then 2 s of silence.
        float gainAfterTone = 0.0f, gainAfterSilence = 0.0f;
        for (int blk = 0; blk < 380; ++blk)
        {
            auto* d = io.getWritePointer (0);
            for (int i = 0; i < block; ++i, ++sampleIndex)
                d[i] = blk < 190 ? 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.0 * sampleIndex / 48000.0)
                                 : 0.0f;
            engine.setParameters (p);
            engine.process (io);

            if (blk == 189) gainAfterTone = 1.0f;      // marker; gain read below
        }
        juce::ignoreUnused (gainAfterTone, gainAfterSilence);

        //  The engine has no public gain probe; assert behaviourally instead:
        //  a tone that resumes after silence must come back at the SAME level
        //  (gain held), not shifted (gain drifted against the noise floor).
        double resumeSq = 0.0; int counted = 0;
        for (int blk = 0; blk < 40; ++blk)
        {
            auto* d = io.getWritePointer (0);
            for (int i = 0; i < block; ++i, ++sampleIndex)
                d[i] = 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.0 * sampleIndex / 48000.0);
            engine.setParameters (p);
            engine.process (io);
            if (blk >= 20)
            {
                for (int i = 0; i < block; ++i)
                    resumeSq += (double) io.getSample (0, i) * io.getSample (0, i);
                counted += block;
            }
        }
        const double resumeDev = std::abs (Decibels::gainToDecibels (
            std::sqrt (resumeSq / counted) / (with_ > 0 ? with_ : 1.0)));
        check (resumeDev < 1.0, "gain held through silence: resumed tone within 1 dB ("
                                    + String (resumeDev, 2) + " dB)");
    }
}

static void testMixNoCombFiltering()
{
    section ("Phase 7: Mix does not comb-filter");

    //  50% mix with all bands bypassed: wet = allpassed clean. Because the
    //  Mix dry leg is the same allpass reference, the sum must stay FLAT.
    //  (Mixing against the raw input would notch around the crossover points.)
    EngineParameters p;
    p.autoLevel = false;
    p.mixPercent = 50.0f;
    for (auto& b : p.bands)
        b.bypass = true;

    double worst = 0.0;
    for (double freq : { 60.0, 120.0, 240.0, 700.0, 1400.0, 4500.0, 9000.0, 15000.0 })
    {
        const double rms = engineSteadyRms (p, freq);
        const double dev = Decibels::gainToDecibels (rms / (0.3 / std::sqrt (2.0)), -120.0);
        worst = jmax (worst, std::abs (dev));
        checkNear (dev, 0.0, 0.2, "mix 50% flat at " + String (freq, 0) + " Hz");
    }
    std::printf ("      worst mix-path deviation: %.4f dB\n", worst);

    //  And with real distortion at 50% mix nothing collapses either: level
    //  stays within the loudness window at a mid frequency near a crossover.
    EngineParameters hot;
    hot.autoLevel = false;
    hot.mixPercent = 50.0f;
    for (auto& b : hot.bands)
        b.drive = 70.0f;

    const double rms700 = engineSteadyRms (hot, 700.0);
    const double dev700 = Decibels::gainToDecibels (rms700 / (0.3 / std::sqrt (2.0)), -120.0);
    check (std::abs (dev700) < 4.0, "50% mix at the crossover with hot drive stays in window ("
                                        + String (dev700, 2) + " dB)");
}

// ================================================================================
// Phase 9 — presets and performance
// ================================================================================
static void testPresets()
{
    section ("Phase 9: factory presets");

    FourColorProcessor proc;
    const int numPresets = proc.getNumPrograms();
    check (numPresets >= 25, "at least 24 musical presets + Default (found "
                                 + String (numPresets) + ")");

    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    //  Every preset must load, produce finite audio, and differ from every
    //  other preset in at least one parameter.
    std::vector<std::vector<float>> signatures;
    MidiBuffer midi;

    for (int i = 0; i < numPresets; ++i)
    {
        proc.setCurrentProgram (i);

        std::vector<float> sig;
        for (auto* param : proc.getParameters())
            if (auto* ranged = dynamic_cast<RangedAudioParameter*> (param))
                sig.push_back (ranged->getValue());
        signatures.push_back (std::move (sig));

        AudioBuffer<float> buf (2, 512);
        bool finite = true;
        int sampleIndex = 0;
        float peak = 0.0f;

        for (int blk = 0; blk < 30; ++blk)
        {
            for (int s = 0; s < 512; ++s, ++sampleIndex)
                for (int c = 0; c < 2; ++c)
                    buf.setSample (c, s, 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.0 * sampleIndex / 48000.0)
                                        + 0.1f * (float) std::sin (2.0 * MathConstants<double>::pi * 3000.0 * sampleIndex / 48000.0));

            proc.processBlock (buf, midi);

            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < 512; ++s)
                {
                    if (! std::isfinite (buf.getSample (c, s))) finite = false;
                    peak = jmax (peak, std::abs (buf.getSample (c, s)));
                }
        }

        check (finite && peak < 4.0f,
               "preset '" + proc.getProgramName (i) + "' produces sane audio (peak "
                   + String (peak, 3) + ")");
    }

    int duplicates = 0;
    for (size_t a = 0; a < signatures.size(); ++a)
        for (size_t b = a + 1; b < signatures.size(); ++b)
        {
            double diff = 0.0;
            for (size_t k = 0; k < signatures[a].size(); ++k)
                diff += std::abs (signatures[a][k] - signatures[b][k]);
            if (diff < 1.0e-4)
                ++duplicates;
        }
    check (duplicates == 0, "no two presets are identical (duplicates: "
                                + String (duplicates) + ")");
}

static void testCpuBudget()
{
    section ("Phase 9: CPU (Release, stereo, 512-sample blocks)");

    for (auto quality : { Quality::high, Quality::ultra })
    {
        FourColorEngine engine;
        engine.prepare (48000.0, 512, 2);

        EngineParameters p;
        p.quality = quality;
        p.bands[0].color = ColorType::warm;
        p.bands[1].color = ColorType::iron;
        p.bands[2].color = ColorType::bite;
        p.bands[3].color = ColorType::fuzz;
        for (auto& b : p.bands)
        {
            b.drive = 60.0f;
            b.behavior = 40.0f;
            b.space = 30.0f;
        }
        engine.setParameters (p);

        AudioBuffer<float> io (2, 512);
        int sampleIndex = 0;

        //  Warm up, then time 10 seconds of audio.
        auto fill = [&]
        {
            for (int i = 0; i < 512; ++i, ++sampleIndex)
                for (int c = 0; c < 2; ++c)
                    io.setSample (c, i, 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 220.0 * sampleIndex / 48000.0));
        };

        for (int blk = 0; blk < 20; ++blk) { fill(); engine.process (io); }

        const int blocks = 48000 * 10 / 512;
        const auto start = Time::getHighResolutionTicks();
        for (int blk = 0; blk < blocks; ++blk)
        {
            fill();
            engine.setParameters (p);
            engine.process (io);
        }
        const double seconds = Time::highResolutionTicksToSeconds (
            Time::getHighResolutionTicks() - start);

        const double audioSeconds = blocks * 512.0 / 48000.0;
        const double realtimeFactor = audioSeconds / seconds;

        std::printf ("      %dx: %.2f s CPU for %.1f s audio = %.1fx realtime\n",
                     oversamplingFactorFor (quality), seconds, audioSeconds, realtimeFactor);

        checkPerformance (realtimeFactor > 8.0, String (oversamplingFactorFor (quality))
                   + "x runs faster than 8x realtime (" + String (realtimeFactor, 1) + "x)");
    }
}

// ================================================================================
// UI regression — the interface must not cost the audio path anything, must
// render at every supported size, and must not disturb state or parameters.
// ================================================================================
static void testEditorSizesAndState()
{
    section ("UI: editor renders at every supported size, state survives");

    FourColorProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    //  Scatter a state, open the editor, and confirm nothing moved.
    auto set = [&proc] (const String& id, float plain)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };
    set (param::band (2, param::color), (float) (int) ColorType::bite);
    set (param::band (2, param::drive), 65.0f);
    set (param::band (3, param::level), -4.0f);
    set (param::xover2, 1200.0f);

    MemoryBlock before;
    proc.getStateInformation (before);

    const std::pair<int, int> sizes[] = { { 900, 560 }, { 980, 620 }, { 1400, 900 } };
    for (auto [w, h] : sizes)
    {
        std::unique_ptr<AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (w, h);

        for (int i = 0; i < 6; ++i)
            MessageManager::getInstance()->runDispatchLoopUntil (10);

        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        check (image.getWidth() == w && image.getHeight() == h,
               "editor renders at " + String (w) + "x" + String (h));

        //  Nothing may be left blank: sample the interior for varied pixels.
        std::set<uint32> distinct;
        for (int y = 10; y < h - 10; y += 7)
            for (int x = 10; x < w - 10; x += 7)
                distinct.insert (image.getPixelAt (x, y).getARGB());
        check (distinct.size() > 40, "  content is drawn at " + String (w) + "x" + String (h)
                                         + " (" + String ((int) distinct.size()) + " distinct samples)");
    }

    MemoryBlock after;
    proc.getStateInformation (after);

    //  The editor stores its own size in the state tree, so compare the
    //  parameters rather than the raw blob.
    FourColorProcessor reloaded;
    reloaded.setStateInformation (after.getData(), (int) after.getSize());
    auto value = [] (FourColorProcessor& p, const String& id)
    {
        auto* q = p.apvts.getParameter (id);
        return q->convertFrom0to1 (q->getValue());
    };
    checkNear (value (reloaded, param::band (2, param::drive)), 65.0, 0.1,
               "opening the editor left band 2 drive untouched");
    checkNear (value (reloaded, param::xover2), 1200.0, 1.0,
               "opening the editor left crossover 2 untouched");
    checkNear (value (reloaded, param::band (3, param::level)), -4.0, 0.1,
               "opening the editor left band 3 level untouched");
}

static void testSpectrumTapIsAudioSafe()
{
    section ("UI: spectrum tap costs the audio thread no allocations");

    FourColorProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    MidiBuffer midi;
    AudioBuffer<float> buffer (2, 512);
    //  An AudioBuffer is not zeroed on construction. Priming with uninitialised
    //  memory made the "frames carry real audio" check below depend on whatever
    //  the allocator handed back, which is why it failed intermittently.
    buffer.clear();
    for (int i = 0; i < 4; ++i)
        proc.processBlock (buffer, midi);

    //  Nobody is draining the FIFO: the writer must stay allocation-free and
    //  finite even when the reader never runs (editor closed).
    allocationCount.store (0);
    trackAllocations.store (true);
    for (int blk = 0; blk < 200; ++blk)
    {
        for (int i = 0; i < 512; ++i)
            for (int c = 0; c < 2; ++c)
                buffer.setSample (c, i, 0.3f * (float) std::sin (0.05 * (blk * 512 + i)));
        proc.processBlock (buffer, midi);
    }
    trackAllocations.store (false);

    check (allocationCount.load() == 0,
           "processBlock with the spectrum tap allocates nothing (counted "
               + String (allocationCount.load()) + ")");

    //  And the reader gets real frames back. Drain first, then push one known
    //  block: with the FIFO left full from the loop above, whatever it still
    //  holds is the OLDEST content, which is not what this check is about.
    std::vector<float> frames (2048 * 2, 0.0f);
    proc.readSpectrumFrames (frames.data(), 2048);

    for (int i = 0; i < 512; ++i)
        for (int c = 0; c < 2; ++c)
            buffer.setSample (c, i, 0.3f * (float) std::sin (0.05 * i));
    proc.processBlock (buffer, midi);

    const int got = proc.readSpectrumFrames (frames.data(), 2048);
    check (got > 0, "spectrum tap delivers frames to the editor (" + String (got) + ")");

    bool nonZero = false;
    for (int i = 0; i < got * 2; ++i)
        nonZero = nonZero || std::abs (frames[(size_t) i]) > 1.0e-6f;
    check (nonZero, "delivered frames carry real audio, not zeros");
}

static void testAnalyzerCpu()
{
    section ("UI: analyzer CPU");

    FourColorProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    std::unique_ptr<AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (980, 620);

    MidiBuffer midi;
    AudioBuffer<float> audio (2, 512);
    int sampleIndex = 0;
    auto pushAudio = [&]
    {
        for (int i = 0; i < 512; ++i, ++sampleIndex)
            for (int c = 0; c < 2; ++c)
                audio.setSample (c, i, 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi
                                                                * 220.0 * sampleIndex / 48000.0));
        proc.processBlock (audio, midi);
    };

    for (int i = 0; i < 10; ++i) { pushAudio(); }
    for (int i = 0; i < 8; ++i) MessageManager::getInstance()->runDispatchLoopUntil (10);

    constexpr int frames = 120;

    auto measure = [&] (Component& target)
    {
        const auto start = Time::getHighResolutionTicks();
        for (int f = 0; f < frames; ++f)
        {
            for (int b = 0; b < 3; ++b) pushAudio();      // ~32 ms of audio
            MessageManager::getInstance()->runDispatchLoopUntil (1);
            auto image = target.createComponentSnapshot (target.getLocalBounds(), true, 1.0f);
            juce::ignoreUnused (image);
        }
        return Time::highResolutionTicksToSeconds (Time::getHighResolutionTicks() - start)
             * 1000.0 / frames;
    };

    //  The analyzer is the only surface that repaints every frame; the rest of
    //  the editor redraws only when something changes. Both are reported.
    auto* fc = dynamic_cast<FourColorEditor*> (editor.get());
    const double analyzerMs = fc != nullptr ? measure (fc->getAnalyzer()) : 0.0;
    const double wholeMs = measure (*editor);

    const double analyzerLoad = analyzerMs * 36.0 / 1000.0 * 100.0;
    const double wholeLoad = wholeMs * 36.0 / 1000.0 * 100.0;

    std::printf ("      analyzer only : %.2f ms/frame -> %.1f%% of one core at 36 FPS\n",
                 analyzerMs, analyzerLoad);
    std::printf ("      whole editor  : %.2f ms/frame -> %.1f%% of one core (worst case,\n"
                 "                      every surface forced to redraw each frame)\n",
                 wholeMs, wholeLoad);

    checkPerformance (analyzerLoad < 15.0, "analyzer repaint stays under 15% of one core ("
                                    + String (analyzerLoad, 1) + "%)");
    checkPerformance (wholeLoad < 40.0, "forced full-editor redraw stays under 40% of one core ("
                                 + String (wholeLoad, 1) + "%)");
}

// ================================================================================
int main()
{
    ScopedJuceInitialiser_GUI juceInit;

    std::printf ("FOUR COLOR test suite\n");

    testParameterLayout();
    testStateRecall();
    testPassthroughAndSafety();
    testNoAllocationInProcess();

    testCrossoverRecombination();
    testCrossoverNull();
    testCrossoverSpacingAndAutomation();

    testColorEnginesDiffer();
    testColorLoudnessMatch();
    testAliasingByQuality();
    testAliasingTypicalCase();
    testColorSwitchAndLatency();

    testCleanReconstruction();
    testSoloMuteLevel();
    testEngineMatrix();
    testQualitySwitchDuringPlayback();
    testQualitySwitchIsSeamless();

    testBehaviorAttackVsBody();
    testBehaviorNoPumpingOnSustained();
    testBehaviorStereoLinked();

    testSpaceIsHarmonicOnly();
    testSpaceMonoBassAndCorrelation();
    testSpaceDecaysAndZeroCost();

    testDriveZeroIsClean();
    testDriveSweepThroughZero();

    testGlobalDrive();
    testGlobalTone();
    testAutoLevel();
    testMixNoCombFiltering();

    testPresets();
    testCpuBudget();

    testEditorSizesAndState();
    testSpectrumTapIsAudioSafe();
    testAnalyzerCpu();

    std::printf ("\n%d checks, %d failed\n", checksRun, checksFailed);
    return checksFailed == 0 ? 0 : 1;
}
