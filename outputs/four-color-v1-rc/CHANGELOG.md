# FOUR COLOR — changelog

## 1.0.0-rc.1 — 2026-08-09

First release candidate. **457 checks, 0 failed**, agreeing across Release,
Debug and ASan/UBSan.

### Fixed

- **Band Tone stepped when a preset was loaded.** `ToneStage` is
  `y = x + amount·highpass(x)` and `amount` was applied per block, so it
  multiplied a non-zero filter output by a different number from one sample to
  the next — a genuine discontinuity of about −38 dBFS. `amount` is now
  smoothed per sample over 20 ms. Isolated by letting one parameter group at a
  time take the preset's value: band tone measured 3.43× the settled slew,
  every other group 1.00–1.09×.

- **BODY changed the hit.** With BODY open through a decay, its gain reached
  the front of the next transient before the envelope detectors noticed it: a
  kick's attack crunch rose 3.6 dB under full BODY. Fixed with an asymmetric
  smoother (closes in 0.3 ms, opens in 3 ms) and a sample-accurate onset guard.
  Now +0.45 dB.

- **BODY was being shut off on sustained material.** ATTACK's transient measure
  reacts to envelope ripple, and a vibrato'd vocal read 30 % transient, a
  beating pad 20 % — closing BODY to 42 % and 48 % on sources that are nothing
  but body. BODY now stands aside for an onset rather than for any rise.

- **The input meter had no source.** Two parallel meter systems existed;
  `inputPeak[]` was never written by anything, so every legacy caller got a
  permanent zero, and a duplicate loop wrote an output copy nobody read.
  `MeterBlock` is now the single source of truth.

- **An idle editor burned about 15 % of one core.** Two causes, both fixed:
  `MeterColumn` repainted unconditionally 30 times a second forever, with
  ballistics decaying to −200 dBFS on a scale that stops at −60; and the
  analyzer's ring still held the last bar of audio after the host stopped
  calling `processBlock`, so its silence test could never fire and it ran two
  4096-point FFTs per frame to animate stale content. Idle went from 87
  repaints per 3 s to **0**.

### Changed

- **BODY's 6 dB is now split between two mechanisms** — 2 dB of pre-gain and
  4 dB of gain on the engine's own non-linear residual — driven by one mask, so
  the control still behaves as one thing. Pre-gain alone could not deliver:
  6 dB of it moves the residual +5.98 dB on a pad and **−0.09 dB on an 808**,
  which was already at the shaper's bound. The ceiling is still 6 dB. The split
  was chosen from a measured sweep.

- **BODY is now measured as density, not level.** The old +2 dB total-RMS
  criterion described the wrong quantity: BODY pushes a saturator and a
  saturator compresses what it is fed. The criterion is now the non-linear
  residual against a clean reference through an identical chain. Weakest source
  +2.16 dB, strongest +5.40 dB.

- **Version is shown in the UI**, under the wordmark.

- **CI no longer tolerates a failing test.** `continue-on-error` was removed
  from the Windows workflow; it had been switched on to accept a known-red
  check, which also meant a real regression could not turn the job red.

### Added

- **100-session audio-thread allocation stress test** across 4 sample rates, 7
  block sizes, mono and stereo, 4 qualities, all 16 Power masks, with colour,
  quality and Power switches mid-stream and the editor open on every third
  session. 0 allocations. Each session is seeded from its index so a failure
  names its own reproduction.

- **Meter calibration tests** over −36 to 0 dBFS, mono and stereo: worst peak
  error 0.0135 dB, worst RMS error 0.0005 dB, trim tracking and clip latch.

- **A real message-loop editor test** measuring frame rate, p95 frame interval,
  repaint counts and dirty area against a no-editor baseline.

- **Power / Solo / Mute semantics tests**: a powered-off band passes exactly
  what Drive 0 passes (0.002 dB), sits 19.1 dB above muted, and can still be
  soloed to audition it clean.

- **Listening pack extended to 140 files** with sub, 808, kick and lead
  sources, Power masks from four active down to all-clean, Auto Level off
  against on at true level, and a mono fold on the low end.

- **176.4 kHz and 256-sample blocks** added to the validation matrix.

### Not fixed, deliberately

- **Residual visualisation** is deferred to 1.1. The seam exists and is empty.
  Nothing is drawn that is not measured.

- **31 presets, not 60–80.** The expansion waits on the listening pass, because
  BODY's character changed in this round.

### Not verified

- Cubase, on either platform.
- Windows RC binary — hosted CI is unavailable on this account.
- pluginval — not installed.
- Signing and notarization — no credentials.

---

## 0.1.0 — earlier rounds

Crossover tree, four colour engines, oversampling, Harmonic Space, the
behaviour axis, the UI rebuild, state versioning, K-weighted Auto Level,
band-relative engine constants, the band workflow round.
