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
    class ColorEngine
    {
    public:
        virtual ~ColorEngine() = default;

        void prepare (double sampleRate, int numChannels);
        void reset();

        //  drivePercent 0..100. Cheap; called once per block.
        void setDrive (float drivePercent) noexcept;

        //  Per-band Behavior modulation multiplies the effective pre-gain.
        //  1.0 = neutral; bounded to about +/-6 dB by the caller.
        virtual void processBlock (float* data, int numSamples, int channel,
                                   const float* preGainMod = nullptr) noexcept = 0;

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
        float blend (float x, float wet) noexcept
        {
            const float e = engageSmoothed.getNextValue();
            return x + e * (wet * compensation - x);
        }

        virtual void prepareInternals() = 0;
        virtual void resetInternals() = 0;
        virtual void driveChanged() noexcept {}

        //  The engine's memoryless transfer approximation, used only to compute
        //  static gain compensation.
        virtual float staticShape (float u) const noexcept = 0;

        //  Maximum pre-gain in dB at drive = 100.
        virtual float maxDriveDb() const noexcept = 0;

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
        void processBlock (float*, int, int, const float*) noexcept override;

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
        void processBlock (float*, int, int, const float*) noexcept override;

    protected:
        void prepareInternals() override;
        void resetInternals() override;
        void driveChanged() noexcept override;
        float staticShape (float u) const noexcept override;
        float maxDriveDb() const noexcept override { return 27.0f; }

    private:
        float fbAmount = 0.0f, evenAmount = 0.0f;
        struct Ch { dsp::OnePole coreLoss; float lastOut = 0.0f; dsp::DcBlocker dc; } ch[maxChannels];
    };

    // --- BITE -----------------------------------------------------------------
    class BiteEngine : public ColorEngine
    {
    public:
        ColorType type() const noexcept override { return ColorType::bite; }
        void processBlock (float*, int, int, const float*) noexcept override;

    protected:
        void prepareInternals() override;
        void resetInternals() override;
        void driveChanged() noexcept override;
        float staticShape (float u) const noexcept override;
        float maxDriveDb() const noexcept override { return 30.0f; }

    private:
        float emphasis = 0.6f;
        struct Ch { dsp::OnePole preHp, postHp; dsp::DcBlocker dc; } ch[maxChannels];
    };

    // --- FUZZ -----------------------------------------------------------------
    class FuzzEngine : public ColorEngine
    {
    public:
        ColorType type() const noexcept override { return ColorType::fuzz; }
        void processBlock (float*, int, int, const float*) noexcept override;

    protected:
        void prepareInternals() override;
        void resetInternals() override;
        void driveChanged() noexcept override;
        float staticShape (float u) const noexcept override;
        float maxDriveDb() const noexcept override { return 36.0f; }

    private:
        float rectify = 0.0f, gateThreshold = 0.002f;
        float envAttack = 0.0f, envRelease = 0.0f;
        struct Ch { float env = 0.0f; dsp::DcBlocker dc; } ch[maxChannels];
    };

    std::unique_ptr<ColorEngine> createColorEngine (ColorType type);
}
