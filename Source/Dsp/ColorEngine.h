// The four colour engines. Deliberately four different STRUCTURES, not one
// waveshaper with four constant sets:
//
//   WARM - memoryless rational soft saturator plus a slow "sag" envelope that
//          eases the drive on sustained loud material, and a drive-dependent
//          bias for even harmonics. Round, fat, gently compressing.
//   IRON - saturator inside a feedback loop whose return passes a one-pole
//          "core loss" filter: the transfer depends on what just happened,
//          which is the transformer-ish density. Level-dependent even term.
//   BITE - asymmetric exponential diode pair with internal pre-emphasis /
//          de-emphasis around the clipper. Fast, forward, upper-mid grit.
//   FUZZ - partial rectification into a wavefolder into a clip, with an
//          envelope gate that sputters on decays. The broken one.
//
// Engines run at the OVERSAMPLED rate inside NonlinearStage; every internal
// time constant is derived from the rate passed to prepare().

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Core/ParameterIds.h"
#include "TptFilters.h"

namespace fourcolor
{
    //  Where a colour engine is sitting in the spectrum.
    //
    //  This is internal state, not a parameter: nothing here is automatable,
    //  nothing is stored in a preset, and no Parameter ID exists for any of it.
    //  It is the answer to a question the engines could not previously ask -
    //  "what frequencies am I actually being asked to distort?" - which is why
    //  a diode pair tuned around 1800 Hz did almost nothing inside a 20-120 Hz
    //  band, and why a gate tuned for a snare chattered on a 30 Hz decay.
    //
    //  It changes whenever a crossover moves, which means anything derived from
    //  it must be smoothed or rate-limited: a coefficient that jumps with the
    //  block is a click on a crossover automation ramp.
    struct ColorContext
    {
        int bandIndex = 0;
        double oversampledRate = 48000.0;
        float bandLowHz = 20.0f;
        float bandHighHz = 16000.0f;
        float centreHz = 500.0f;
    };

    class ColorEngine
    {
    public:
        virtual ~ColorEngine() = default;

        void prepare (double sampleRate, int numChannels);
        void reset();

        //  drivePercent 0..100. Cheap; called once per block.
        void setDrive (float drivePercent) noexcept;

        //  Where this engine sits in the spectrum. Called at block rate, and
        //  NEVER resets internal state: a crossover move must not silence a
        //  feedback loop or empty a sag envelope. Engines that derive
        //  coefficients from it do so through contextChanged().
        void setContext (const ColorContext& c) noexcept;

        const ColorContext& getContext() const noexcept { return context; }

        //  Per-band Behavior modulation. `preGainMod` multiplies the effective
        //  pre-gain; `residualMod` multiplies the NONLINEAR RESIDUAL the engine
        //  contributes. Both are per-sample linear factors around 1.0, at the
        //  oversampled rate, and both are optional.
        virtual void processBlock (float* data, int numSamples, int channel,
                                   const float* preGainMod = nullptr,
                                   const float* residualMod = nullptr) noexcept = 0;

        //  Static make-up so Drive changes colour more than loudness. Derived
        //  from each engine's memoryless curve at a -12 dBFS reference input.
        //  The engine applies this itself, inside the dry/wet blend below, so
        //  that Drive = 0 is exactly unity.
        float getCompensationGain() const noexcept { return compensation; }

        //  0 at Drive = 0, 1 from Drive = 5% upwards. See kEngageDrive01.
        float getEngageTarget() const noexcept { return engageTarget; }

        virtual ColorType type() const noexcept = 0;

        //  Drive below this fraction fades the whole engine out; at and above
        //  it the engine is fully engaged and behaves exactly as before, so no
        //  preset with Drive > 5% is affected by the contract.
        static constexpr float kEngageDrive01 = 0.05f;

    protected:
        //  The one place the dry/wet contract is expressed. `wet` is the
        //  engine's compensated output; at Drive 0 this returns x untouched,
        //  bit for bit, with no residual, no gate and no DC filtering.
        //
        //  Written out, this is  x + e * r * (nonlinear residual), so
        //  `residualGain` scales ONLY what the shaper added and never the clean
        //  signal underneath it. That is what makes BODY's second mechanism
        //  possible: once a source is already deep in saturation, more pre-gain
        //  buys almost no new harmonic content, but the content that is there
        //  can still be made more audible relative to the clean signal.
        //
        //  Doing it here rather than after the tone stage matters. Here `x` is
        //  the engine's own input, so the difference is purely nonlinear
        //  product; after ToneStage::processPost the difference would also
        //  contain the tone filter's linear response, and scaling it would turn
        //  BODY into a hidden second Tone control at Tone +/-100.
        float blend (float x, float wet, float residualGain = 1.0f) noexcept
        {
            const float e = engageSmoothed.getNextValue();
            return x + e * residualGain * (wet * compensation - x);
        }

        virtual void prepareInternals() = 0;
        virtual void resetInternals() = 0;
        virtual void driveChanged() noexcept {}

        //  Recompute anything derived from the band's frequency range. Called
        //  only when the context has actually moved by an audible amount, so an
        //  engine may do real work here - but it must not reset state.
        virtual void contextChanged() noexcept {}

        //  The engine's memoryless transfer approximation, used only to compute
        //  static gain compensation.
        virtual float staticShape (float u) const noexcept = 0;

        //  Maximum pre-gain in dB at drive = 100.
        virtual float maxDriveDb() const noexcept = 0;

        ColorContext context;

        double rate = 48000.0;
        int channelCount = 2;
        float d01 = 0.25f;
        float preGain = 1.0f;
        float compensation = 1.0f;
        float engageTarget = 1.0f;

        //  Smoothed per sample: a block-rate step in the blend would be an
        //  audible click on a fast Drive automation ramp through the low end.
        juce::SmoothedValue<float> engageSmoothed { 1.0f };

        static constexpr int maxChannels = 2;

    private:
        void updateCompensation() noexcept;
    };

    // --- WARM -----------------------------------------------------------------
    class WarmEngine : public ColorEngine
    {
    public:
        ColorType type() const noexcept override { return ColorType::warm; }
        void processBlock (float*, int, int, const float*, const float*) noexcept override;

    protected:
        void prepareInternals() override;
        void resetInternals() override;
        void driveChanged() noexcept override;
        float staticShape (float u) const noexcept override;
        float maxDriveDb() const noexcept override { return 24.0f; }

    private:
        float bias = 0.0f, biasOut = 0.0f, sagDepth = 0.0f;
        struct Ch { dsp::OnePole sagEnv; dsp::DcBlocker dc; } ch[maxChannels];
    };

    // --- IRON -----------------------------------------------------------------
    class IronEngine : public ColorEngine
    {
    public:
        ColorType type() const noexcept override { return ColorType::iron; }
        void processBlock (float*, int, int, const float*, const float*) noexcept override;

    protected:
        void prepareInternals() override;
        void resetInternals() override;
        void driveChanged() noexcept override;
        void contextChanged() noexcept override;
        float staticShape (float u) const noexcept override;
        float maxDriveDb() const noexcept override { return 27.0f; }

    private:
        //  The core-loss corner follows the band. A fixed 280 Hz was the centre
        //  of the LOW MID band and nothing else: inside a 20-120 Hz band it
        //  passes the whole signal, so the loop returned everything and IRON had
        //  no density down there at all.
        void updateCoreLoss() noexcept;

        float fbAmount = 0.0f, evenAmount = 0.0f;
        struct Ch { dsp::OnePole coreLoss; float lastOut = 0.0f; dsp::DcBlocker dc; } ch[maxChannels];
    };

    // --- BITE -----------------------------------------------------------------
    class BiteEngine : public ColorEngine
    {
    public:
        ColorType type() const noexcept override { return ColorType::bite; }
        void processBlock (float*, int, int, const float*, const float*) noexcept override;

    protected:
        void prepareInternals() override;
        void resetInternals() override;
        void driveChanged() noexcept override;
        void contextChanged() noexcept override;
        float staticShape (float u) const noexcept override;
        float maxDriveDb() const noexcept override { return 30.0f; }

    private:
        //  Same story: 1800 Hz was the HIGH MID centre. In the low band a
        //  1800 Hz high-pass passes almost nothing, so BITE's defining
        //  pre-emphasis did nothing there and it collapsed to a bare diode.
        void updateEmphasisFilters() noexcept;

        float emphasis = 0.6f;
        struct Ch { dsp::OnePole preHp, postHp; dsp::DcBlocker dc; } ch[maxChannels];
    };

    // --- FUZZ -----------------------------------------------------------------
    class FuzzEngine : public ColorEngine
    {
    public:
        ColorType type() const noexcept override { return ColorType::fuzz; }
        void processBlock (float*, int, int, const float*, const float*) noexcept override;

    protected:
        void prepareInternals() override;
        void resetInternals() override;
        void driveChanged() noexcept override;
        void contextChanged() noexcept override;
        float staticShape (float u) const noexcept override;
        float maxDriveDb() const noexcept override { return 36.0f; }

    private:
        //  The gate's clock follows the band. 0.5 ms is right for a snare and
        //  wrong for a 30 Hz decay, where an envelope that fast rides the
        //  waveform and the gate opens and shuts twice per cycle - buzz, not
        //  sputter.
        void updateGateTimes() noexcept;

        float rectify = 0.0f, gateThreshold = 0.002f;
        float envAttack = 0.0f, envRelease = 0.0f;
        struct Ch { float env = 0.0f; dsp::DcBlocker dc; } ch[maxChannels];
    };

    std::unique_ptr<ColorEngine> createColorEngine (ColorType type);
}
