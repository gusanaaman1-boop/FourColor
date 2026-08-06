# FOUR COLOR — workflow round, test report

```
STATUS: PARTIAL
```

Two acceptance numbers are missed and both are stated below rather than
smoothed over. Everything else in the brief is done and measured.

## BASELINE

- commit: `1e2b7a4` (recorded in docs/BASELINE-WORKFLOW.md)
- checks: Release 369 / 1 failed, Debug 369 / 1 failed
- builds: VST3, AU, Standalone — Release and Debug, 0 warnings with -Werror
- auval: PASS · pluginval: not installed · Cubase: not run

## COMMITS

| hash | phase |
| --- | --- |
| `395fd74` | 0 — audit and baseline |
| `da6e9f6` | 1+2 — band headers, powered-off bands drain their colour |
| `0eab58c` | analyzer to 30 FPS (the bigger plot cost more per frame) |
| `b45cfa4` | 3+4 — SHAPE gets a real BODY; input meter reads the right point |
| `4a348b2` | 5+6 — meters flank the analyzer, globals move to a MASTER drawer |
| `cc9067b` | 7+8 — sixteen power masks, four SHAPE presets, tooltips |
| `cc9067b` | 9 — test candidate |

## PARAMETER IDS

```
added:   none
removed: none
changed: none
```

As expected. Power is the existing `bN_bypass` (ON = bypass false), SHAPE is
the existing `bN_behavior` at its existing −100…+100 range, the MASTER drawer
holds the existing `globalDrive` and `globalTone`, and the two trims are the
existing `input` and `output`. 51 parameters throughout.

## DSP RESULTS

| | |
| --- | --- |
| BODY decay lift | bass **+2.74 dB**, 808 +1.70, pad +1.15, vocal +0.57 |
| BODY attack window | **+0.00 dB** on all four sources |
| BODY silent tail | **−200 dBFS**; −66 dBFS noise floor rises **0.000 dB** |
| ATTACK crunch vs centre | WARM +1.53, IRON +0.88, BITE +1.05, FUZZ +2.28 dB, all monotonic |
| SHAPE automation | no step above the material's own |
| mono / stereo | both, all sample rates and block sizes in the matrix |
| preset RMS change | 0.000 dB across all 27 pre-existing presets |

## BAND POWER

| | |
| --- | --- |
| 16-mask result | all finite; **no mask puts a hole in the spectrum** |
| clean reconstruction | all four off leaves the spectrum uncoloured to **0.093 dB** |
| click measurement | toggle adds no step the material did not have (0.36221 vs 0.36549) |

## METERS

| | |
| --- | --- |
| input tap | moved to **after Input Trim, before the crossover** — it was before the trim and did not move with it |
| output tap | after Output Trim and after Safety, unchanged (already correct) |
| published | per-channel peak + block mean-square + clip latch, lock-free |
| ballistics | GUI-side only: instant peak attack, 20 dB/s decay, ~300 ms RMS, 1.5 s hold |
| clip / reset | latches at 0 dBFS, click to clear |
| allocations | **0** on the audio thread |

## GUI

| | |
| --- | --- |
| screenshots | `screenshots/` — 11, including 900×560, 980×620, 1400×900 |
| analyzer CPU | **11.3%** of one core at 30 FPS (was 14.2% before this round) |
| whole editor | 21.8% worst case (was 27.7%) |
| repaint | band headers and meters repaint themselves, not the window |

The analyzer grew from 29% to 39.5% of the window when the band cards were
removed, which pushed repaint to 15.3% and over budget. It runs at 30 FPS now —
the bottom of the brief's own 30–45 range — rather than the picture being made
coarser. No threshold was moved.

## VALIDATION

| | |
| --- | --- |
| tests | **381 checks, 2 failed** — Release, Debug and ASan/UBSan agree |
| sanitiser findings | **0** |
| auval | **PASS** |
| pluginval | not installed, not run, not claimed |
| Windows CI | run 31030970761 built before this round; push refreshes it |
| Cubase | **NOT RUN** on either platform |

## INSTALLED

```
~/Library/Audio/Plug-Ins/VST3/FourColor.vst3
~/Library/Audio/Plug-Ins/Components/FourColor.component
```

Universal arm64 + x86_64, 9.6 MB.

## BACKUPS

```

```

Contains the previous FourColor.vst3 and FourColor.component. Nothing was
deleted; no Cubase cache was touched; no user project was modified.

## USER TEST PACKAGE

```
outputs/four-color-workflow-test/
├─ screenshots/              11 PNGs
├─ listening-renders/        82 loudness-matched WAVs
├─ preset-fingerprint.txt    all 31 presets, RMS / peak / DC
├─ FOUR_COLOR_TEST_GUIDE_HE.md
├─ TEST_REPORT.md
└─ KNOWN_ISSUES.md
```

## KNOWN ISSUES

See KNOWN_ISSUES.md. In short: BODY reaches the 2 dB target on one source of
four because the brief's own 6 dB depth cap does not survive saturation; one
loudness criterion carried from the previous round; Cubase unrun; pluginval
not installed.
