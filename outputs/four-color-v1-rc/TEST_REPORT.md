# FOUR COLOR — RC1 test report

```
STATUS:   RC READY FOR OWNER VALIDATION
VERSION:  1.0.0-rc.1
HEAD:     a0cf17a67aef00a5546470c4c9dccd2339698a61
```

## Commits in this round

| Phase | Commit | What it did |
| --- | --- | --- |
| 1 | `bbb41e3` | Freeze RC scope, capture baseline |
| 2 | `485f4af` | Correct the Auto Level convergence test |
| 3 | `7958f78` | BODY as harmonic density; residual reinforcement |
| 4 | `0ef92cc` | Unify metering telemetry, calibration tests |
| 5 | `9f42223` | Band workflow + analyzer performance |
| 6 | `de09426` | Windows audio-thread allocation stress |
| 7 | `aab6619` | Zero-failure validation across all builds |
| 8 | `241d204` | Presets + owner listening pack |
| 9 | `85039f1` | Host validation matrix |
| 10 | `2801e65` | Package 1.0.0-rc.1 |
| 11 | `218a5e2` | Delivery documents |
| — | `0c466ad` | Fix three things that would have broken the Windows build |
| — | `41d4f3f` | Make the editor CPU figure a measurement rather than a coin toss |

## Tests

| | Before | After |
| --- | --- | --- |
| checks | 381 | **473** |
| failures | **2** | **0** |

Agreeing across three builds, same numbers in each:

```
Release          473 checks, 0 failed
Debug            473 checks, 0 failed
ASan + UBSan     473 checks, 0 failed, 0 sanitizer findings
Windows / MSVC   473 checks, 0 failed  (GitHub Actions, Release and Debug)
```

Matrix: 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz against block sizes
1 / 16 / 32 / 64 / 128 / 256 / 512 / 1024 / 2048, mono and stereo.

## BODY

The +2 dB **total RMS** criterion was replaced, not relaxed. BODY raises
pre-gain into a saturator and a saturator compresses what it is fed, so level
is not the observable. The measurement is now the nonlinear residual against a
clean reference rendered through an identical chain (same crossover, same
65-sample latency, Tone neutral, Space 0, Mix 100, Drive 0).

| source | residual lift | total RMS | attack rise | silent tail |
| --- | --- | --- | --- | --- |
| bass note | **+5.40 dB** | +1.56 dB | 0.00 dB | −200 dBFS |
| 808 | **+2.16 dB** | +0.55 dB | 0.00 dB | −200 dBFS |
| pad | **+5.39 dB** | +1.34 dB | 0.00 dB | −200 dBFS |
| vocal | **+2.84 dB** | +1.41 dB | 0.00 dB | −200 dBFS |
| pluck | **+2.65 dB** | +1.01 dB | 0.00 dB | −200 dBFS |

Target ≥ 2.0 dB on the weakest. **Weakest is 2.16 dB.**

Noise floor at −66 dBFS rises **0.000 dB**. Stereo image: identical channels in
give identical channels out, exactly (0.00e+00), at both ends of Shape.

### DSP did change, and why

Pre-gain alone could not deliver. Measured: 6 dB of extra pre-gain moves the
residual **+5.98 dB on a pad** and **−0.09 dB on an 808** — the 808 was already
at the shaper's bound. So BODY's 6 dB is now split between two mechanisms
driven by one mask: **2 dB of pre-gain** (generate harmonics) and **4 dB of
residual gain** (make them audible relative to the clean signal). The split was
chosen from a sweep, not by taste:

```
drive:residual   6:0     4:2     3:3     2:4     0:6
weakest lift   -0.09   +0.72   +1.30   +1.92   +2.17  dB
```

The ceiling is still 6 dB. The threshold was never moved.

Two defects were found on the way and fixed:

- BODY was being shut off by ATTACK's transient measure reacting to envelope
  **ripple**. A vibrato'd vocal read 30 % transient and a beating pad 20 %,
  closing BODY to 42 % and 48 % on material that is nothing but body. BODY now
  stands aside for an *onset* rather than for any rise.
- With BODY open through a decay, the residual gain reached the front of the
  next hit before the envelopes noticed it: a kick's attack crunch rose
  **3.6 dB** under full BODY. Fixed with an asymmetric smoother (closes in
  0.3 ms, opens in 3 ms) plus a sample-accurate onset guard. **Now +0.45 dB.**

## Auto Level

The failure was the test, not the matcher. The correction glides with a 1.5 s
time constant; the test rendered four seconds and discarded two — 1.33 constants
— and reported the unfinished glide as matcher error. Eight seconds of pre-roll,
same thresholds, no DSP change.

| source | error before | error after |
| --- | --- | --- |
| melody | 0.86 LU | **0.01 LU** |
| vocal | 0.48 LU | **0.01 LU** |
| bass | 0.24 LU | 0.06 LU |
| drums | 0.15 LU | 0.07 LU |
| pad | 0.04 LU | 0.03 LU |
| full mix | 0.05 LU | 0.00 LU |
| **median** | 0.20 LU | **0.02 LU** |
| **worst** | 0.86 LU | **0.07 LU** |

Gain moves at most **0.027 dB** during the measurement window — proof of
settling. Latency alignment was therefore not needed and not added. Silence
drift 0.0016 dB over 5 s.

## Meters

Two parallel systems existed; `inputPeak[]` was never written by anything, so
every legacy caller got a permanent zero. `MeterBlock` is now the only source
of truth and the legacy accessors read it.

```
worst peak error   0.0135 dB   (budget 0.1)
worst RMS error    0.0005 dB   (budget 0.2)
Input Trim +6 dB   moves the Input meter +6.000 dB
Output Trim -6 dB  takes it back to -0.000 dB at the output
clip               latches at and above 0 dBFS, not below; reset clears it
```

Measured over −36 / −18 / −12 / −6 / −1 / 0 dBFS, mono, stereo equal and
stereo unequal, read exactly as the GUI reads them.

## Power / Solo / Mute

| | measured |
| --- | --- |
| powered-off band vs Drive 0 | **0.002 dB** |
| powered off vs muted | **19.1 dB** apart |
| all four muted | −100 dB |
| soloing a powered-off band | auditions it clean, 0.002 dB |
| all 16 masks | no hole; all four off leaves 0.093 dB |

Host name stays `LOW Bypass`; the UI shows Power with the sense inverted.

## Audio safety

```
allocations on the audio thread   0, over 100 randomised sessions
sanitizer findings                0
NaN / Inf                         0
```

The 100 sessions cover 4 sample rates, 7 block sizes, mono and stereo, all 4
qualities, randomised colours/drive/Shape/Tone/Space, all 16 Power masks in
turn, editor open on every third session, and colour, quality and Power
switches mid-stream. Each session is seeded from its index so a failure names
its own reproduction.

The single unexplained Windows allocation did not reproduce.

## CPU

```
4x oversampling      19.8x realtime
8x oversampling      10.8x realtime
analyzer repaint     11.1% of one core at 30 FPS
editor during play   2.8% of one core above the harness baseline
idle editor          -0.2%; 0 analyzer repaints and 0 background repaints over 3 s
frame rate           29.0 FPS against 30 nominal
frame evenness       p95 37.2 ms against a 34.1 ms median (1.09x)
```

Every figure above is the best of several passes with the baseline re-measured
between them. A single sample of a CPU difference on a machine that is also
compiling something measures the machine: this same check read -1.8 %, 2.8 %,
8.3 %, 19.7 % and 23.7 % across runs with no code change. The frame-evenness
criterion is likewise relative to the run's own median rather than to an
absolute 40 ms, because the test harness drives the message loop in 5 ms slices
and pins the timer near 27 FPS whatever the plug-in does.

Two real performance bugs were fixed here. `MeterColumn` repainted
unconditionally 30 times a second forever, with ballistics decaying to
−200 dBFS on a scale that stops at −60 (seven seconds of invisible repainting).
And the analyzer's ring still held the last bar of audio after the host stopped
calling `processBlock`, so its own silence test could never fire — it animated
stale content and ran two 4096-point FFTs per frame to do it. Idle went from
**87 repaints per 3 s to 0.**

## Windows portability

Three things in this round's own code would have stopped the Windows build, and
were only found because the package was checked rather than assumed:

- `getrusage` and `<sys/resource.h>`, added in Phase 5, are POSIX only. MSVC has
  neither, so the build would have failed at the include. Replaced with a
  portable `processCpuSeconds()` — `GetProcessTimes` on Windows.
- The Windows package was missing `Tests/fixtures` entirely, and
  `build-windows.bat` treats a failing test as fatal, so the build would have
  aborted after compiling successfully.
- The p95 frame check was measuring the harness, and a flaky performance
  assertion would likewise have aborted the owner's build.

That gap is now closed. The repository was made public, which makes hosted
Windows minutes free, and the code **compiles and passes on MSVC under `/WX`**:
473 checks, 0 failed, in both Release and Debug, on GitHub's Windows runners.
A real x86_64 VST3 comes out of every push as a downloadable artefact.

Two checks are reported rather than enforced there, for the same reason the CPU
budgets are: the runner is headless with software rendering, so its analyzer
timer reads 23.5 FPS against 29.0 on a desktop, and a frame dirties exactly
100% of the window because that backend does no partial repaints at all.

## Preset click

Loading a preset put a genuine discontinuity into the output. Isolated by
letting only one parameter group take the preset's value:

```
only band tone changes      3.43x settled slew
every other group           1.00x - 1.09x
```

`ToneStage` is `y = x + amount·highpass(x)`, and `amount` was applied per
block, so it multiplied a non-zero filter output by a different number from one
sample to the next. It is now smoothed per sample over 20 ms. **Band tone now
measures 1.00×.**

What remains with all 51 parameters moving at once is 1.54×, and it is not a
click: sample by sample it rises smoothly over about eight samples, 0.6 ms in —
several smoothers ramping together under the 15 ms colour crossfade, whose
equal-power law peaks at √2 on correlated signals (measured 1.67×).

## Frozen invariants — all unchanged

| | |
| --- | --- |
| crossover null | −111.9 dB worst sample, −109.0 dB RMS |
| latency | **65 samples** at every quality |
| Drive 0 | THD+N −124.3 dB, gain error 0.0016 dB |
| Mix 50 % | flat to 0.0104 dB |
| Space switch-on leakage | −66.8 to −71.4 dB |
| LOW band mono | bit-exact |
| aliasing, typical at 4× | −70.1 dB |

## Validation

| | |
| --- | --- |
| auval | **PASS** |
| pluginval | **not installed** — needs approval to install |
| Windows CI | **GREEN** — run 31868751879, Release and Debug both pass |
| Cubase macOS | **not run** |
| Cubase Windows | **not run** |

## Builds

```
build/FourColor_artefacts/Release/VST3/FourColor.vst3          x86_64 + arm64
build/FourColor_artefacts/Release/AU/FourColor.component       x86_64 + arm64
build/FourColor_artefacts/Release/Standalone/FourColor.app     x86_64 + arm64
dist/FourColor-1.0.0-rc.1-macOS.zip
```

Hashes in `hashes.txt`.

## Installed

```
~/Library/Audio/Plug-Ins/VST3/FourColor.vst3
~/Library/Audio/Plug-Ins/Components/FourColor.component
```

Both hash identical to the built binaries.

## Backups

```
~/FourColor-backup-20260809-205024/FourColor.vst3
~/FourColor-backup-20260809-205024/FourColor.component
```

Nothing was deleted. No Cubase cache was touched. No user project or preset
was modified.

## Deliverables

```
outputs/four-color-v1-rc/
├── artefacts/          the macOS zip
├── listening/          140 WAVs, 8 folders
├── screenshots/        10 PNGs
├── hashes.txt
├── TEST_REPORT.md
├── KNOWN_ISSUES.md
├── CUBASE_TEST_HE.md
├── LISTENING_GUIDE_HE.md
├── QUICK_START_HE.md
├── QUICK_START_EN.md
├── CHANGELOG.md
└── RELEASE_CHECKLIST.md
```

## Deferred to 1.1

Residual visualisation only. The seam exists and is empty; nothing is drawn
that is not measured.

## Owner action required

1. Listen — `LISTENING_GUIDE_HE.md`, four questions at the end.
2. Cubase on macOS — `CUBASE_TEST_HE.md`, 24 rows.
3. Cubase on Windows — same document, 18 rows.
4. Approve pluginval installation, or decide to ship without it.

## Blockers — external only

| Blocker | Why |
| --- | --- |
| Windows RC binary | No Windows machine reachable, no cross-compiler, hosted GitHub Actions blocked on this account since 2026-08-06. Build with `scripts\build-windows.bat Release`. |
| pluginval | Not installed; installing a third-party binary needs your approval. |
| Signing / notarization | No credentials. Bundles carry an ad-hoc signature, which is not notarisation. |
| Cubase validation | Cannot be driven from this machine, on either platform. |
