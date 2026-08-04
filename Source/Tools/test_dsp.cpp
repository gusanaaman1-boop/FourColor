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
#include <vector>

#include <juce_gui_extra/juce_gui_extra.h>

#include "../Core/ParameterIds.h"
#include "../Dsp/Crossover.h"
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

    double rmsOf (const AudioBuffer<float>& b, int channel = 0)
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

    double dcOf (const AudioBuffer<float>& b, int channel = 0)
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

    // Neutral settings: output ~= input once smoothing settles.
    {
        AudioBuffer<float> ref;
        AudioBuffer<float> buffer (2, block);

        for (int blockIndex = 0; blockIndex < 20; ++blockIndex)
        {
            auto sine = makeSine (2, block, 997.0, sr);
            ref = sine;
            buffer = sine;
            proc.processBlock (buffer, midi);
        }

        double maxError = 0.0;
        for (int i = 0; i < block; ++i)
            maxError = jmax (maxError, (double) std::abs (buffer.getSample (0, i) - ref.getSample (0, i)));

        check (maxError < 1.0e-4, "neutral chain is transparent (max error " + String (maxError, 8) + ")");
    }

    // Silence in -> silence out, finite always.
    {
        AudioBuffer<float> buffer (2, block);
        buffer.clear();
        proc.processBlock (buffer, midi);
        check (peakOf (buffer) < 1.0e-6f, "silence stays silent");
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

    std::printf ("\n%d checks, %d failed\n", checksRun, checksFailed);
    return checksFailed == 0 ? 0 : 1;
}
