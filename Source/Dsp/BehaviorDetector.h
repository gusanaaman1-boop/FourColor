// SHAPE: the BODY <-> ATTACK axis. Two envelope followers on the same stereo-
// linked signal - one fast, one slow - feeding two DIFFERENT mechanisms, one
// per side, both modulating the colour engine's pre-gain:
//
//   ATTACK (+): transient-driven. The fast/slow difference raises pre-gain on
//               the hit, so the attack gets crunch and the tail stays calm.
//
//   BODY   (-): DEFICIT-driven, and not merely the mirror of ATTACK. It asks
//               how far the signal has fallen below its own recent reference,
//               and lifts exactly that shortfall into the engine - so decays,
//               sustains and the quiet half of a phrase get colour they were
//               not getting, while the initial transient is left alone.
//
//               Reducing transient drive, which is all the old negative side
//               did, made the hit duller without ever making the body denser.
//
// Both sides are bounded, smoothed per sample, and gated so that silence and
// noise floors are never lifted.
//
// Time constants scale per band: a "fast" 1 ms follower on a 40 Hz band would
// just track the waveform, so the low bands use slower clocks than the top.
// Modulation depth is bounded to +/-6 dB and smoothed, which is what keeps
// this from becoming a pumping compressor.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Core/ParameterIds.h"

namespace fourcolor
{
    class BehaviorDetector
    {
    public:
        BehaviorDetector() = default;

        void prepare (double sampleRate, int bandIndex);
        void reset();

        //  behavior in [-1, 1]. At 0 the modulation is unity, but the detector
        //  keeps running - see writeModulation.
        void setBehavior (float behavior) noexcept { amount = juce::jlimit (-1.0f, 1.0f, behavior); }

        //  Reads the (pre-processing) band buffer and writes TWO per-sample
        //  linear factors, each of length n, both stereo-linked (one curve for
        //  both channels, so the image can never lean):
        //
        //    driveOut     multiplies the colour engine's pre-gain
        //    residualOut  multiplies the nonlinear residual the engine adds
        //
        //  MUST be called on every block, including when the amount is 0. The
        //  envelopes are the detector's memory of the last few hundred
        //  milliseconds; freezing them means that moving the control off zero
        //  starts from a picture of whatever was playing when it was last
        //  touched, which on a running transport is simply wrong.
        void writeModulation (const juce::AudioBuffer<float>& bandInput,
                              float* driveOut, float* residualOut, int n) noexcept;

        //  Whether the modulation does anything. NOT a reason to skip the call.
        bool isActive() const noexcept { return std::abs (amount) > 1.0e-3f; }

    private:
        float amount = 0.0f;

        //  One-pole coefficients (attack/release pairs) for the two followers
        //  and the modulation smoother.
        float fastAtk = 0.0f, fastRel = 0.0f;
        float slowAtk = 0.0f, slowRel = 0.0f;
        float modSmooth = 0.0f;

        float fastEnv = 0.0f, slowEnv = 0.0f, modState = 1.0f;

        //  BODY's reference: a slow peak-following envelope that falls far more
        //  slowly than it rises, so it remembers how loud the passage has just
        //  been. The deficit is measured against this, not against a fixed
        //  level, which is what keeps BODY musical across a quiet verse and a
        //  loud chorus alike.
        float referenceEnv = 0.0f;
        float referenceRise = 0.0f, referenceFall = 0.0f;

        //  The residual curve gets its own smoother, and unlike the drive
        //  curve that smoother is ASYMMETRIC: it closes almost instantly and
        //  opens at the ordinary rate.
        //
        //  BODY has to be out of a hit's way before the hit arrives. With the
        //  symmetric 3 ms smoother the gain was still several decibels open
        //  into the first milliseconds of a kick, and the kick's measured
        //  attack crunch rose 4 dB under full BODY - the control was changing
        //  the transient, which is exactly what it promises not to do.
        //  Closing over 0.3 ms fixes that; opening stays at 3 ms so the return
        //  after the transient is inaudible.
        float residualState = 1.0f;
        float residualFall = 0.0f;

        //  ATTACK depth. +/-6 dB was measured to move the kick's attack-crunch
        //  spread only 1.3 dB, too subtle for the product's headline control.
        static constexpr float maxModDb = 9.0f;

        //  BODY's total authority, unchanged at 6 dB, but now spent on TWO
        //  mechanisms instead of one.
        //
        //  Pre-gain alone cannot deliver density on material that is already
        //  saturated. Measured on an 808 at Drive 55, six decibels of extra
        //  pre-gain moved the nonlinear residual by -0.09 dB: the shaper was
        //  already near its bound, so the extra push produced almost no new
        //  harmonic content. The same six decibels on a pad moved it +5.98 dB.
        //  That is not a tuning problem, it is what a bounded curve does.
        //
        //  So the authority is split: part of it raises pre-gain (generate
        //  harmonics) and part of it scales the residual the engine produces
        //  (make them audible relative to the clean signal). The second
        //  mechanism works regardless of how deep into the curve the material
        //  already sits.
        //
        //  The 2/4 weighting is measured, not chosen. Sweeping the split with
        //  everything else held (weakest nonlinear-residual lift across bass,
        //  808, pad, vocal and pluck):
        //
        //      drive:residual    6:0     4:2     3:3     2:4     0:6
        //      weakest lift    -0.09   +0.72   +1.30   +1.92   +2.17  dB
        //
        //  Monotonic, because the pre-gain half is the unreliable one: worth
        //  +5.97 dB on a bass note and -0.09 dB on an 808, entirely depending
        //  on how saturated the source already was. The residual half pays the
        //  same everywhere. The weighting follows that, and the easy sources
        //  lose nothing they need - bass still measures +5.74 dB and pad
        //  +5.48 dB at 2/4.
        //
        //  The total is still 6 dB. The acceptance threshold was never moved.
        static constexpr float maxBodyDb = 6.0f;
        static constexpr float bodyDriveShareDb    = 2.0f;
        static constexpr float bodyResidualShareDb = 4.0f;
        static_assert (bodyDriveShareDb + bodyResidualShareDb <= maxBodyDb,
                       "BODY's two mechanisms must stay inside the 6 dB ceiling");

        //  How much of BODY's depth applies to ANY non-transient material,
        //  before the deficit term adds the rest.
        //
        //  A purely deficit-driven rule cannot colour a steady pad at all: a
        //  sustained tone sitting at its own reference has no deficit, so the
        //  lift measured 0.26 dB against a 2 dB target. But "drive the body
        //  harder" is the control's whole promise, and a pad IS body. So BODY
        //  is a baseline on everything that is not a transient, plus more where
        //  the signal has actually fallen away.
        static constexpr float bodyBaseline = 0.55f;

        //  12 dB, not 18. Measured on a decaying bass note, an 18 dB window
        //  only reached a third of full depth through the musically useful part
        //  of the decay, and the lift came out at 1.3 dB against a 2 dB target.
        //  A saturator also gives back less than it is fed, so the pre-gain
        //  window has to close sooner than the output target suggests.
        static constexpr float bodyRangeDb = 12.0f;

        //  Absolute floor. Below this the lift is faded out entirely, whatever
        //  the relative deficit says - this is what stops BODY raising a noise
        //  floor or a room tone in a gap. -60 dBFS, faded smoothly over the
        //  12 dB above it rather than switched, because a hard gate on a gain
        //  path is a click.
        //  -45 dBFS, faded over the 15 dB above it. -60 was too low: the tail
        //  of a bass note spends a long time between -60 and -48 dBFS, and the
        //  lift was still partly open down there. Anything below -45 dBFS is a
        //  tail or a room, not body worth colouring.
        static constexpr float bodyFloorDb = -45.0f;
        static constexpr float bodyFadeDb = 15.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BehaviorDetector)
    };
}
