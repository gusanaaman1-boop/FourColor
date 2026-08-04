# FOUR COLOR — architecture

Working title: **FOUR COLOR**. A four-band colour/saturation processor: a spiritual
successor to the idea of a four-band fuzz, not a port of anyone's code, UI, names or
artwork. Four bands, four saturation characters, one dynamic Behavior axis, one
harmonic-only Space, and very few knobs.

## Repository context (Phase 0 audit)

| Item | Finding |
| --- | --- |
| Workspace | `/Users/gussa/Make Music` — one directory per plug-in, no umbrella repo |
| Sibling projects | CoreColor, TRIX, DualSpace, DigiMeter, MeloTrace, Scope, Riser, Imprint |
| JUCE | `~/JUCE`, **JUCE 9.0.0**, shared by every plug-in here (reuse rule) |
| Build | CMake ≥ 3.22 (`cmake 4.4.0` installed), Xcode/clang, no Ninja |
| Convention | `juce_add_plugin` + separate `juce_add_console_app` test and screenshot tools |
| Company / codes | `COMPANY_NAME "Naaman"`, manufacturer code `Naam`, unique 4-char plug-in code |
| Formats | macOS: VST3 + AU + Standalone. Windows: VST3 + Standalone |
| macOS binary | universal `arm64;x86_64` (some hosts here run under Rosetta) |
| Per-project git | DigiMeter / TRIX / DualSpace / Scope each have their own `.git`; CoreColor does not |
| pluginval | **not installed on this machine** — validation is the in-tree test suite + `auval` |

Baseline: `FourColor/` did not exist before this session, so there was no pre-existing
build to preserve. The toolchain baseline was taken from the sibling `DigiMeter` tree,
which has current Release artefacts built by the same CMake/JUCE/clang combination, and
was re-proved by the Phase 1 skeleton build in this project.

This project follows the same conventions. Plug-in code: `Fclr`. Product name `FourColor`,
displayed as **FOUR COLOR**.

## Signal flow

```
input
 └─ Input Trim ─────────────────────────────────────────────────┐ (dry tap)
     └─ 4-band crossover (LR4 tree, allpass-compensated)        │
         ├─ band 0 LOW ─┐                                       │
         ├─ band 1 LMID ├─ per band:                            │
         ├─ band 2 HMID │    Behavior detector (dual envelope)   │
         └─ band 3 HIGH ┘    ├─ pre-colour Tone (emphasis)   ┐   │
                             │   └─ oversampled colour engine │  │
                             │       └─ post Tone (de-emph)   ┘  │
                             ├─ Harmonic Space send (residual)   │
                             ├─ band Mix (vs delay-aligned clean)│
                             └─ band Level / Solo / Mute / Bypass│
         └─ sum of four bands                                    │
             └─ Global Tone (tilt)                               │
                 └─ Auto Level (slow, bounded)                   │
                     └─ Dry/Wet against the latency-aligned dry ─┘
                         └─ Output Trim
                             └─ safety (NaN/Inf scrub + soft ceiling)
```

`processBlock()` allocates nothing, takes no locks, opens no files and touches no
GUI object. Every buffer is sized in `prepareToPlay()` for the largest supported
block and oversampling factor. `ScopedNoDenormals` is active for the whole call.

## Crossover — why a naive tree is not enough

Three cascaded LR4 splits do **not** recombine flat. An LR4 low-pass plus its LR4
high-pass sums to a **second-order allpass** at that crossover, not to unity. In a
tree, the low half picks up the allpass of crossover 1 and the high half picks up
the allpass of crossover 3, and the two halves then no longer add back to the
allpass of crossover 2 — you get a dip around the mid split.

The fix used here is the standard tree compensation: cross-apply the *sibling*
allpass to the other half.

```
lowHalf  = LP(f2, x)              highHalf = HP(f2, x)
band0    = LP(f1, lowHalf)        band2    = LP(f3, highHalf)
band1    = HP(f1, lowHalf)        band3    = HP(f3, highHalf)
band0,1 += AP(f3)                 band2,3 += AP(f1)      <-- compensation
```

Then

```
Σ bands = AP(f3)·AP(f1)·[LP(f2)+HP(f2)] = AP(f1)·AP(f2)·AP(f3)·x
```

which is a pure allpass cascade: **magnitude is flat by construction**, and the only
difference from the input is phase. Phase 2 measures this rather than assuming it.

All sections are TPT/ZDF state-variable filters (Zavalishin topology). They stay
stable under coefficient modulation, which is what makes dragging a crossover handle
during playback safe. Cutoffs are smoothed and coefficients are recomputed on a
32-sample control grid, not per sample.

Minimum spacing between adjacent crossovers is enforced as a ratio (1.30×) so the
handles cannot cross or collapse onto each other.

## Colour engines

Four genuinely different structures, not one `tanh` with four constant sets. Each
implements the same `ColorEngine` interface (`prepare / setParameters / processSample /
reset / gainCompensation`) so a fifth can be added without touching the band processor.

| | WARM | IRON | BITE | FUZZ |
| --- | --- | --- | --- | --- |
| Core | rational soft saturator `x/(1+\|x\|)` | saturator with **feedback loop and memory** | asymmetric **exponential diode** pair | clip + partial rectify + wavefold |
| Knee | very wide, never fully clips | wide, level dependent | short | abrupt |
| Memory | slow "sag" reduces drive on sustained loud material | one-pole core-loss filter inside the feedback path | none (fast, transient-following) | gate envelope |
| Even harmonics | drive-dependent bias | asymmetric loop offset | asymmetry between the two diode legs | rectification amount |
| Character felt as | fat, round, compressed | dense, heavy, punchy | present, gritty, fast | broken, grainy, gated |

Feedback-around-a-saturator (IRON) is bounded because the saturator itself is
bounded, and the loop gain is kept below 1. FUZZ and the asymmetric engines are
followed by a DC blocker; the tests assert residual DC.

Every engine also reports a static `gainCompensation(drive)` so that turning Drive up
does not simply turn the band up, and so Auto Level is not left doing all the work.

## Oversampling

`juce::dsp::Oversampling`, FIR equiripple, wrapped per band around only the nonlinear
section (pre-emphasis → shaper → de-emphasis). Behavior detection, Space and the
crossover run at base rate, which is both correct and much cheaper than oversampling
the whole chain.

| Quality | factor | notes |
| --- | --- | --- |
| Draft | 1× | no oversampling, no added latency |
| Normal | 2× | |
| High | 4× | **default** |
| Ultra | 8× | |

All four bands share the same factor, so all four have identical latency and the
recombination stays sample-aligned. Latency is measured from the oversampler at
`prepareToPlay()`, reported to the host, and used to delay the dry path. Quality
changes are applied at a block boundary through a full re-prepare, never mid-buffer.

The nonlinear section is called through a `shapeBlock()` seam so antiderivative
antialiasing (ADAA) can be dropped in later without changing the band processor.

## Behavior — BODY ↔ ATTACK

Per band, two envelope followers on the same signal: a fast one and a slow one, with
time constants scaled per band (the low band is slower than the high band, because a
40 Hz cycle is 25 ms long and a "fast" 1 ms detector on it just follows the waveform).
The transient measure is the bounded ratio between them.

- **ATTACK (+)** — drive rises with the transient measure, so the stick/pick/click
  distorts harder while the tail stays calmer.
- **BODY (−)** — drive is ducked by the transient measure, so the transient passes
  comparatively clean and the saturation is heard on the sustain.

The detector is stereo-linked (it uses the louder channel) so the stereo image does
not move. Modulation is bounded to ±6 dB of pre-drive and one-pole smoothed, which is
what stops it from turning into a pumping compressor. Attack/release are internal —
they are not exposed as knobs at this stage.

## Harmonic Space

Not a reverb. Space works on the **nonlinear residual** only:

```
g        = <processed, clean> / <clean, clean>     (slow running least squares)
residual = processed − g·clean
```

`g` is the running best linear fit between the clean band and the processed band, so
`residual` is what is genuinely new — the harmonics the colour engine created. Fitting
`g` instead of assuming it is what keeps the subtraction honest when the engine's
linear gain changes with drive and level.

The residual feeds a short micro-diffusion: a pair of allpass diffusers plus a small
set of short taps, per-band time range (roughly 18–35 ms low, 3–12 ms high), bounded
feedback, damping in the loop, and gentle L/R decorrelation. There is no tail.

Band 0 (and any band whose top edge sits under ~150 Hz) is forced mono in the Space
path, so mono compatibility of the low end is structural rather than a setting. At
`SPACE = 0` the whole engine is skipped, not just faded out.

## Auto Level

Slow, bounded, and internal. Long-window loudness proxy of the pre-processing and
post-processing signal, silence-gated, one-pole smoothed over roughly a second, and
clamped to ±12 dB. It never writes to the visible Output parameter and never appears
as automation.

## Classes

| File | Responsibility |
| --- | --- |
| `Dsp/TptFilters.h` | TPT/ZDF SVF, one-pole, DC blocker, fractional delay |
| `Dsp/Crossover.*` | 4-band LR4 tree with allpass compensation |
| `Dsp/ColorEngine.*` | interface + WARM / IRON / BITE / FUZZ |
| `Dsp/BehaviorDetector.*` | dual-envelope transient measure |
| `Dsp/ToneStage.*` | band-scoped pre-emphasis / de-emphasis pair |
| `Dsp/HarmonicSpace.*` | residual estimate + micro-diffusion |
| `Dsp/AutoLevel.*` | slow bounded loudness match |
| `Dsp/BandProcessor.*` | one band end to end, owns its oversampler |
| `Dsp/FourColorEngine.*` | crossover + 4 bands + global stage + dry/wet |
| `Core/ParameterIds.h` | frozen parameter ID strings |
| `Core/PresetLibrary.*` | the 24 factory presets, in code |
| `Ui/*` | theme, knob, band display, band strip, global bar |
| `Tools/test_dsp.cpp` | the measurement suite |
| `Tools/screenshot_tool.cpp` | deterministic UI screenshots |

## Explicitly out of scope for this 70% stage

Delay, LFO, sequencer, pitch, full reverb, convolution, external sidechain, per-band
M/S, arbitrary band counts, bitcrusher, sample-rate reduction, generated presets, AI
analysis, licensing, installer, cloud presets, skins.
