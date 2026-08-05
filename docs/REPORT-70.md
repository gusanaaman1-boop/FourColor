# FOUR COLOR — 70% completion report

Date: 2026-08-05. One session, Phases 0–9 complete, one commit per phase.

## 1. Build status

| Target | Release | Debug |
| --- | --- | --- |
| VST3 | ✅ builds | ✅ builds |
| AU (macOS) | ✅ builds, **auval PASS** | — |
| Standalone | ✅ builds, launches, alive after 5 s | ✅ builds |
| FourColorTests | ✅ **290 checks, 0 failed** | — |
| FourColorShot | ✅ 6 reference screenshots | — |

## 2. Built artefact paths

- `build/FourColor_artefacts/Release/VST3/FourColor.vst3` (9.2 MB, universal arm64+x86_64)
- `build/FourColor_artefacts/Release/AU/FourColor.component` (9.1 MB)
- `build/FourColor_artefacts/Release/Standalone/FourColor.app` (11 MB)
- `build-debug/FourColor_artefacts/Debug/…` (VST3 + Standalone)
- Installed for local hosts: `~/Library/Audio/Plug-Ins/VST3/FourColor.vst3`, `~/Library/Audio/Plug-Ins/Components/FourColor.component`

## 3. Commits by phase

```
dbf1491 Phase 0: repository audit, architecture and frozen parameter map
4925c63 Phase 1: plugin skeleton, frozen APVTS layout, state recall, safety stage
35921d0 Phase 2: allpass-compensated LR4 four-band crossover
6ed222e Phase 3: four colour engines + oversampled nonlinear stage
b71d7c4 Phase 4: per-band chain wired end to end
5152da9 Phase 5: BODY/ATTACK behavior system
3a14ae4 Phase 6: Harmonic Space - residual-only micro-diffusion
2efc673 Phase 7: global integration
baea2a2 Phase 8: functional GUI
(this commit) Phase 9: presets, validation, report
```

## 4. DSP class structure

```
FourColorEngine            whole path, host-independent (tests drive it directly)
 ├─ Crossover              LR4 tree + sibling-allpass compensation (TPT SVF)
 ├─ BandProcessor ×4       one band end to end
 │   ├─ BehaviorDetector   dual envelope, stereo-linked, per-band clocks
 │   ├─ ToneStage          pre/post split ±9 dB shelf at band centre
 │   ├─ NonlinearStage     4 engines + 4 oversamplers + OS-domain crossfade
 │   │   └─ ColorEngine    WARM / IRON / BITE / FUZZ (four structures)
 │   ├─ HarmonicSpace      2-basis LLS residual + micro-diffusion
 │   └─ Thiran clean-align delay, mix/level/mute/bypass/solo fades
 ├─ AutoLevel              slow bounded loudness match
 └─ tilt, dry delays, Thiran wet align, safety stage
```

## 5. Crossover recombination results

- Magnitude: flat to **0.0001 dB worst case** (30 Hz–18 kHz probes incl. all cut points).
- Null vs analytic AP(f1)·AP(f2)·AP(f3): **-111.9 dB worst sample, -109 dB RMS** (float32
  rounding floor; criteria -95/-100 dB, calibration documented in the test).
- Spacing enforcement (1.30×, f2 authoritative) verified; f2 sweep during playback
  click-free (max step 0.031 on a 0.5-amplitude sine).

## 6. Aliasing by quality (worst case: FUZZ, drive 85, hot input)

| Quality | worst audible-band alias |
| --- | --- |
| Draft 1x | -19.7 dB |
| Normal 2x | -25.1 dB |
| High 4x (default) | **-42.1 dB** |
| Ultra 8x | **-60.4 dB** |

Typical material (BITE, drive 60, -12 dBFS): **-70.1 dB** at default quality.
Known and documented: the half-band decimator's transition band folds content just above
Nyquist to just below it (measured at 23.1 kHz, -29.9 dB) — inherent to half-band FIR
oversampling, above audibility, and unchanged by the factor; a steeper final decimation
filter is 30%-phase work if wanted.

## 7. Latency and alignment

- Reported latency: 0 / 49 / 60 / 65 samples at 1x/2x/4x/8x (48 kHz), integer contract:
  wet padded to ceil(L) by a Thiran fractional delay, dry delayed ceil(L).
- Global bypass measured as input delayed exactly by the reported latency (error < 1e-5).
- Mix uses the crossover's **allpass reference** as its dry leg → 50% mix measured flat
  to 0.13 dB worst case (no comb filtering). Clean reconstruction (all bands bypassed)
  flat to 0.011 dB. Lagrange interpolation was replaced by Thiran after measuring
  -0.44 dB droop at 12 kHz.

## 8. State recall

- Full APVTS round-trip verified (scattered values incl. quality, crossovers, colors,
  behavior, solo, and the `selectedBand` UI property).
- **Cubase recall not tested in this session — no Cubase on this Mac.** auval's state
  tests pass; host-level recall remains on the 30% list.

## 9. Presets (26 musical + Default)

Bass: Sub Weight · Rolling Bass Body · Acid Bite · Mid Bass Iron · Bass Harmonic Lift ·
Controlled Bass Fuzz
Drums: Kick Weight · Kick Attack · Drum Bus Warmth · Crunchy Loop · Snare Bite ·
Parallel Fuzz Drums
Synths: Warm Pad · Dirty Pad Halo · Melodic Lead Bite · Dark Stab Iron · Acid Destruction ·
Upper Harmonic Lift
Vocals: Vocal Edge · Dark Vocal Grit · Harmonic Air · Telephone Fuzz
Mix: Gentle Four-Band Glue · Low-End Safe Saturation · Brightness Without EQ · Parallel Color

All hand-written in `PresetLibrary.cpp` with a stated musical intent per preset; the test
suite loads each one, asserts sane finite audio and that no two are identical.

## 10. CPU (Release, stereo, 512 blocks, all four engines driven + behavior + space)

| Quality | realtime factor |
| --- | --- |
| High 4x | **20.2× realtime** (~5% of one core) |
| Ultra 8x | **13.5× realtime** (~7.4% of one core) |

Audio thread allocation count across the 80-configuration matrix: **0** (proved by a
global-operator-new tripwire, which also caught a real per-block String allocation in
Phase 1).

## 11. Screenshots

`ui-shots/`: min 900×560, default 980×620, HMID selected, HIGH selected, 1400×900, init
state. Rendered deterministically by `FourColorShot`.

## 12. Acceptance criteria — PASSED

- Four bands recombine flat (0.0001 dB) with phase-proving null test
- Four colours measurably different in spectrum (pairwise log-spectral distance 8.6–26.4 dB),
  not just level (static compensation holds ±2.5 dB across the drive range)
- Drive causes no uncontrolled level jumps (loudness window test, all engines × 5 drives)
- BODY vs ATTACK clearly different (1.98 dB crunch spread on the hit, monotone; sustained
  material moves < 0.6 dB — no pumping)
- Space adds depth from the distortion only (hot/quiet relative-halo ratio 9.6×; no tail;
  LOW halo bit-exact mono; correlation 0.90)
- Parallel Mix comb-free (0.13 dB worst)
- No clicks under automation (crossover sweep, colour switch, quality switch tests)
- State recalls exactly (in-process; Cubase pending)
- Zero allocations on the audio thread
- No fake controls: every UI element is attached to a parameter or real data
- VST3 + AU + Standalone build; auval passes; standalone stays alive
- 26 musical presets

## 13. Acceptance criteria — NOT met / not verifiable here

- **Cubase state recall / scan**: no Cubase on this machine.
- **pluginval**: not installed on this machine (no-payments rule → not fetched without
  approval; it is free, so it can be added next session if wanted).
- Block size 1 with **8x** quality is exercised only in the quality-switch test, not the
  full matrix (matrix runs at High 4x).

## 14. Known issues

1. Half-band near-Nyquist fold (see §6) — inaudible, documented.
2. 1x (Draft) aliasing is honest-but-present (-19.7 dB worst case); Draft is labelled
   Draft for a reason.
3. The colour crossfade runs both engines for 15 ms; a colour switch during extreme FUZZ
   gating can slightly change the gate envelope's phase (inaudible in tests, but
   unmeasured on real material).
4. Band level leaks ±2 dB near a cut due to LR4 slope overlap (physics, documented in test).
5. Editor A/B slots are not persisted in the plugin state (deliberate: state stays
   preset-compatible; revisit in the 30%).
6. `resized()` of the S/M/B buttons is also called from `mouseDrag` to track handle moves —
   correct but worth a tidier layout pass.

## 15. The remaining 30%

- Listening-driven DSP tuning of all four engines and the Behavior clocks (real material,
  real monitors — measurements here are proxies).
- Cubase 15 (Windows) verification: scan, automation, state recall; the family's
  windows.yml CI pattern; x86 ASan pass per the workspace's UB lessons.
- pluginval (macOS + Windows) once approved to install.
- FFT input analyzer behind the crossover display (design allows it; deferred on purpose).
- Auto Level refinement (loudness proxy → K-weighted, per-colour static tables).
- CPU: NEON/SSE of the per-sample band loop if 8x on older machines needs it.
- Preset expansion + preset browser with categories (TopBar combo is flat).
- A/B persistence, Undo/Redo via UndoManager wiring.
- macOS notarised installer + Windows installer (per the workspace's installer lessons).
- GUI polish pass (spacing, animation restraint, DPI > 150% verification).
