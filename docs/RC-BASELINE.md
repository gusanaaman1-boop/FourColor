# FOUR COLOR — Release Candidate baseline

Captured at the start of the RC round, before any change in it. Every number
here is what the tree measured, not what it was hoped to measure.

## Tree

| | |
| --- | --- |
| HEAD | `150442b75e20c236e0567e1c673b3af10fb95782` |
| branch | `main` |
| dirty at capture | `outputs/FOUR-COLOR-REVIEW-ALL-IN-ONE.md` (untracked deliverable) |
| JUCE | 9.0.0 — `857aab9c4eb3084af639a380a693dcec7d728b73` (2026-07-22) |
| CMake | 4.4.0 |
| macOS compiler | Apple clang 21.0.0 (clang-2100.1.1.101) |
| macOS architectures | `arm64;x86_64` universal |
| Windows compiler | MSVC v143, Visual Studio 17 2022, `-A x64`, `/WX`, on the pinned `windows-2022` runner |
| project version | 0.1.0 (raised to 1.0.0-rc.1 in Phase 10) |

## Artefact paths

```
build/FourColorTests_artefacts/Release/FourColorTests
build/FourColor_artefacts/Release/VST3/FourColor.vst3
build/FourColor_artefacts/Release/AU/FourColor.component
build/FourColor_artefacts/Release/Standalone/FourColor.app
build-debug/FourColorTests_artefacts/Debug/FourColorTests
build-san/FourColorTests_artefacts/Debug/FourColorTests      (ASan + UBSan)
```

## Test suite

**381 checks, 2 failed.** Both failures are acceptance criteria, not crashes.

### Failure 1 — BODY decay lift

```
bass note  decay +2.74 dB   attack +0.00 dB   tail -200.0 dBFS (neutral -200.0)
808        decay +1.70 dB   attack +0.00 dB   tail -200.0 dBFS (neutral -200.0)
pad        decay +1.15 dB   attack +0.00 dB   tail -200.0 dBFS (neutral -200.0)
vocal      decay +0.57 dB   attack +0.00 dB   tail -200.0 dBFS (neutral -200.0)
FAIL  BODY adds at least 2 dB to the decay on every source (weakest 0.57 dB)
```

The criterion asks for +2 dB of **total RMS** in the decay window. BODY works by
raising pre-gain into a saturator, and a saturator compresses what is pushed
into it, so total level is the wrong observable for it. Phase 3 replaces the
measurement; the raw numbers above are kept so the replacement can be compared
against them.

### Failure 2 — Auto Level worst-case loudness error

```
bass       loudness error: 0.00 LU off -> 0.24 LU on
melody     loudness error: 4.20 LU off -> 0.86 LU on
drums      loudness error: 0.70 LU off -> 0.15 LU on
pad        loudness error: 0.39 LU off -> 0.04 LU on
vocal      loudness error: 3.10 LU off -> 0.48 LU on
full mix   loudness error: 0.09 LU off -> 0.05 LU on
ok    median loudness error is under 0.35 LU (0.20)
FAIL  worst loudness error is under 0.75 LU (0.86)
```

The gain glide is 1.5 s. The test ran 4 s and discarded the first 2 — about
1.33 time constants, so the correction was still moving when it was measured.
Phase 2 gives it 8 s of pre-roll before measuring and keeps both thresholds.

## Everything else that is already green

| | |
| --- | --- |
| crossover recombination | flat to 0.0001 dB, null at -111.9 dB |
| latency | 65 samples at every Quality setting |
| Drive 0 | THD+N -124.3 dB, gain error 0.0016 dB |
| Mix 50 % | flat to 0.0104 dB |
| aliasing, typical material at 4x | -70.1 dB |
| Space switch-on leakage | -67.8 dB; LOW band mono bit-exact |
| all 16 Power masks | no hole; all four off leaves 0.093 dB of colour |
| Auto Level drift in silence | 0.0016 dB over 5 s |
| CPU | 19.8x realtime at 4x, 10.8x at 8x |
| analyzer repaint | 11.1 % of one core at 30 FPS |
| audio-thread allocations | 0 on macOS; one unexplained single event on Windows |
| sanitizers | 0 findings, ASan + UBSan |
| auval | PASS |

## Decisions frozen for this round

1. `bN_bypass` keeps its Parameter ID and its host-facing name. The UI calls it
   Power and inverts it: **Power on = bypass false**.
2. Power off passes that frequency range through clean. It is not a mute, and it
   does not widen any neighbouring band.
3. The analyzer stays at 30 FPS. It is not going back to 36.
4. Residual visualisation is deferred to 1.1. The seam exists and stays empty —
   nothing is drawn that is not measured.
5. 31 presets are enough for the RC if each one is musical and loads cleanly.
   Expansion waits for the owner's listening pass, because BODY's character is
   still open.
6. No new parameters and no Parameter ID changes, in either direction.
