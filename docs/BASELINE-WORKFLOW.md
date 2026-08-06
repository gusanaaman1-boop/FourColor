# FOUR COLOR — baseline before the workflow round

Recorded before any edit, so every later claim in this round has something to
be measured against.

## 1. Starting point

| | |
| --- | --- |
| HEAD | `1e2b7a4befe7b9c0adff5d60fe40b9a4aec46c18` |
| Subject | "Windows is built, and this time there is a run ID behind the claim" |
| Date | 2026-08-06 06:48 +0300 |
| Working tree | clean |

## 2. Builds — all green

| Target | Result |
| --- | --- |
| VST3 Release | `build/FourColor_artefacts/Release/VST3/FourColor.vst3` |
| AU Release | `build/FourColor_artefacts/Release/AU/FourColor.component` |
| Standalone Release | `build/FourColor_artefacts/Release/Standalone/FourColor.app` |
| VST3 / AU / Standalone Debug | built |
| Warnings from FOUR COLOR sources, `-Werror` on | **0** |
| Windows x64 VST3 + Standalone | built by CI run 31030970761, MSVC, `/WX` clean |

## 3. Tests

```
Release   369 checks, 1 failed
Debug     369 checks, 1 failed
```

The one failure is the documented `worst loudness error is under 0.75 LU (0.86)`
on the melody source — see `REPORT-RC.md` section 5. **Any other failure during
this round is a regression caused by this round.**

## 4. Validation

| | |
| --- | --- |
| `auval -v aufx Fclr Naam` | **PASS** |
| `pluginval` | **NOT INSTALLED** — no run, no claim |
| Cubase | **NOT RUN** on either platform |

## 5. Screenshots

`outputs/baseline-workflow/` — 11 PNGs, including the three sizes the brief
asks for:

- `fourcolor-min-900x560.png`
- `fourcolor-default-980x620.png`
- `fourcolor-large-1400x900.png`

plus one per selected band, a hover-on-DRIVE frame, a drag-on-BEHAVIOR frame
and a high-SPACE frame.

## 6. Parameter map — 51 parameters, all frozen

**Global (11):** `input`, `globalDrive`, `globalTone`, `autoLevel`, `mix`,
`output`, `quality`, `bypassed`, `xover1`, `xover2`, `xover3`

**Per band (10 x 4 = 40),** as `b0_`…`b3_` + suffix: `color`, `drive`,
`behavior`, `tone`, `space`, `bandMix`, `level`, `solo`, `mute`, `bypass`

The controls this round moves or renames map onto existing IDs and **nothing
is added**:

| New control | Existing parameter |
| --- | --- |
| Band header Power `⏻` | `bN_bypass` — ON means bypass = false |
| Band header `S` | `bN_solo` |
| Band header `M` | `bN_mute` |
| `SHAPE` (renamed from Behavior) | `bN_behavior`, range −100…+100 unchanged |
| Input Trim beside the input meter | `input` |
| Output Trim beside the output meter | `output` |
| MASTER drawer contents | `globalDrive`, `globalTone` |

## 7. Existing infrastructure to reuse, not duplicate

**Meters.** `PluginProcessor` already publishes per-channel block peaks through
`std::atomic<float> inputPeak[2]` / `outputPeak[2]`, drained by
`readAndResetInputPeak(channel)` / `readAndResetOutputPeak(channel)`.
`GlobalBar` already draws a two-channel segmented meter with visual decay and a
peak hold. Phase 4 extends this; it does not start again.

**One real finding already.** The input meter currently taps at the very top of
`processBlock`, which is **before** input trim — the trim is applied inside
`FourColorEngine::process`. The brief requires the input meter to read *after*
Input Trim and *before* the crossover. That is a genuine bug in the existing
meter, not just a layout change, and Phase 4 has to move the tap.

The output meter taps after `engine.process`, which is already after Output
Trim and after the safety stage — that one is correct as it stands.

**Analyzer.** `Ui/Analyzer` owns the FFT and the lock-free spectrum tap
(`readSpectrumFrames`). Phase 4 must not add a second FFT; the meters need only
block peak and sum-of-squares.

**Band buttons.** `Ui/BandCards` already hosts `S`/`M`/`B` as real
`ButtonAttachment`s per band. Phase 1 relocates and restyles these; the
attachments and IDs stay.

## 8. What must still be true at the end of this round

Carried forward from the previous round, and re-asserted by the suite:

- crossover recombination flat to 0.0001 dB, null at −111.9 dB
- reported latency exactly **65 samples** in every Quality
- Drive 0 clean: THD+N −124.3 dB, gain error 0.0016 dB
- Mix 50% flat to 0.0104 dB
- Space switch-on leakage −67.8 dB; LOW band halo bit-exact mono
- BODY/ATTACK monotonic, spread 1.6–6.2 dB
- 0 audio-thread allocations
- 0 sanitiser findings
