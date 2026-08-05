# FOUR COLOR — UI rebuild to the reference image

Date 2026-08-05. Four commits, DSP untouched, 304/304 checks pass.

## 1. Files changed

**Added**
`Source/Ui/Design.h` · `Source/Ui/Analyzer.h/.cpp` · `docs/UI-CONTROL-MAP.md` · this report

**Rewritten**
`Source/Ui/Theme.h/.cpp` · `Source/Ui/Knob.h/.cpp` · `Source/Ui/TopBar.h/.cpp` ·
`Source/Ui/BandCards.h/.cpp` · `Source/Ui/BandStrip.h/.cpp` · `Source/Ui/GlobalBar.h/.cpp` ·
`Source/PluginEditor.h/.cpp`

**Extended (audio-safe additions only)**
`Source/PluginProcessor.h/.cpp` — lock-free mid/side spectrum FIFO, per-channel atomic peaks
`Source/Tools/test_dsp.cpp` — UI regression + analyzer CPU section
`Source/Tools/screenshot_tool.cpp` — QA variant set
`CMakeLists.txt` — source list

**Deleted** `Source/Ui/CrossoverDisplay.h/.cpp` (replaced by `Analyzer`)

**Not touched at all**: every file under `Source/Dsp/` and `Source/Core/`.

## 2. Commits

```
cf2fba7 UI 1/4 - design system
9f068a2 UI 2/4 - layout
5d8a97d UI 3/4 - analyzer and interaction
(this)  UI 4/4 - final wiring and QA
```

## 3–7. Screenshots (`ui-shots/`)

| File | What it shows |
| --- | --- |
| `fourcolor-min-900x560.png` | minimum size |
| `fourcolor-default-980x620.png` | default size |
| `fourcolor-large-1400x900.png` | large size |
| `fourcolor-band1-low.png` … `-band4-high.png` | each of the four bands selected |
| `fourcolor-hover-drive.png` | hover on DRIVE |
| `fourcolor-drag-behavior-attack.png` | BEHAVIOR dragged towards ATTACK (+68) |
| `fourcolor-space-high.png` | SPACE / SPREAD at 86% |
| `fourcolor-init.png` | silence — nothing animating |

All rendered by `FourColorShot`, deterministically, with a broadband programme.

## 8. Build results

| Target | Result |
| --- | --- |
| VST3 Release / Debug | ✅ / ✅ |
| AU Release | ✅ — **auval PASS** |
| Standalone Release / Debug | ✅ / ✅ — launches, alive after 6 s |
| FourColorTests | ✅ |
| FourColorShot | ✅ |

## 9. Test results

**304 checks, 0 failed** (was 290 before the UI work; 14 new UI checks).
All pre-existing DSP checks — crossover recombination, null test, engine spectra, aliasing by
quality, behavior, space, latency, comb-free mix, presets, CPU, allocation tripwire — still pass
unchanged.

New checks:
- editor renders at 900×560, 980×620 and 1400×900, with real content at each size
- opening the editor leaves drive, crossover and level parameters untouched
- `processBlock` with the spectrum tap allocates nothing, even with the editor closed and the
  FIFO never drained
- the tap delivers non-zero frames to the editor
- analyzer CPU (below)

## 10. Explicit confirmation — DSP and parameter IDs unchanged

- `git diff` over the whole UI work touches **no file under `Source/Dsp/` or `Source/Core/`**.
- No line containing `ParameterID`, `layout.add` or a `param::` id was added, removed or
  modified in `PluginProcessor.cpp`.
- The suite still measures **51 host-visible parameters** and full state recall of
  `input`, `mix`, `quality`, `xover1`, `xover3`, band drive, colour, behavior, solo and the
  `selectedBand` UI property.
- All 27 factory presets load and produce sane audio (unchanged test).
- The only processor additions are a lock-free `AbstractFifo` write and atomic peak stores.

## 11. Analyzer CPU

| Measurement | ms/frame | share of one core at 36 FPS |
| --- | --- | --- |
| Analyzer repaint (the only surface that redraws every frame) | 3.94 | **14.2 %** |
| Whole editor forced to redraw every frame (worst case, not what happens live) | 7.86 | 28.3 % |

First measured at 15.2 % for the analyzer, over the 15 % bar. Rather than move the bar, the
paint was optimised: each band's body and contours are now built as one mirrored path (one
fill and one stroke per band instead of two), columns went 320 → 240, and the side contours
went 3 → 2. Audio-side cost of the tap is a FIFO write per sample.

## 12. Remaining differences from the reference image

1. **Harmonic-residual layer** — the reference implies a second, fainter harmonic contour.
   The engine does not expose a residual measurement to the GUI, and the brief forbids
   inventing data, so it is not drawn. `Analyzer::setResidualProvider()` is the seam; wiring
   `HarmonicSpace`'s existing residual to it is a small, honest follow-up.
2. **Per-engine spectrum character** (BITE showing sharper transient peaks, FUZZ grainier
   edges) follows from the real audio rather than from engine-specific drawing rules.
3. **Font** — Inter is not bundled; the platform's geometric sans is used with the same
   tracking discipline. Bundling Inter is a licensing/asset decision.
4. **DRIVE sparks** are deterministic (fixed seed) and scale with drive and measured level;
   they do not re-randomise per frame, so they rest completely in silence.
5. The reference's analyzer shows a denser, more "waveform-like" low end; ours reflects the
   actual test programme. With musical material the shapes match closely.
6. Band-card knob sizes cap on very large windows so a 1400×900 window gains whitespace
   rather than oversized controls.
