# FOUR COLOR — control map for the reference-image rebuild

Baseline before this work: commit `976d345`, 290/290 DSP checks pass, screenshot
`ui-shots/fourcolor-band-hmid.png` at 980×620.

**No parameter ID changes.** Every ID below already exists and is frozen since Phase 1
(`docs/PARAMETERS.md`). No DSP file is modified; the only engine-side additions are
lock-free read taps for the analyzer and meters.

## Existing control → new location

| Parameter ID | Old location | New location (reference image) |
| --- | --- | --- |
| `bypassed` | TopBar text button | TopBar far right, **power symbol** |
| `quality` | TopBar combo "High 4x" | TopBar right, combo reading `HIGH 4x` |
| (program) | TopBar combo | TopBar centre, `‹ name* ›` preset browser |
| (A/B, undo) | TopBar buttons | TopBar centre-right, `A / B` + undo arrow |
| `xover1/2/3` | display handles | Analyzer capsule handles + value above |
| `b<n>_drive` | selected-band panel only | **Band card small knob** + selected-panel big DRIVE |
| `b<n>_level` | thumb on card meter | **Band card small knob** `LEVEL` + panel small LEVEL |
| `b<n>_color` | panel radio list | Band card caption (engine name) + panel `COLOR` list |
| `b<n>_solo/mute/bypass` | card buttons | Band card `S` `M` `B`, bottom-left |
| `b<n>_behavior` | panel slider | Panel centre, **BODY ↔ ATTACK** horizontal slider |
| `b<n>_tone` | panel knob | Panel `TONE` with DARK/BRIGHT captions |
| `b<n>_space` | panel knob `SPACE / SPREAD` | Panel `SPACE / SPREAD`, teal, expanding arcs |
| `b<n>_bandMix` | panel knob | Panel right group `MIX`, after a vertical divider |
| `input` | global bar | Global strip, after the L/R input meter |
| `globalDrive` | global bar | Global strip `GLOBAL DRIVE` |
| `autoLevel` | rect/round button | Global strip, **round amber ring toggle** |
| `globalTone` | global bar | Global strip `GLOBAL TONE` |
| `mix` | global bar | Global strip `MIX` |
| `output` | global bar | Global strip `OUTPUT`, before the L/R output meter |

Controls removed: none. Controls added: none (the band cards surface `drive`/`level`,
which already existed, as a second attachment to the same parameter).

## New non-parameter infrastructure (audio-safe)

| Addition | Thread safety |
| --- | --- |
| M/S spectrum tap (`AbstractFifo`, interleaved mid/side) | lock-free, pre-allocated |
| per-channel input/output peaks | `std::atomic<float>`, relaxed |
| per-band output peaks (already present) | `std::atomic<float>`, relaxed |

`processBlock` gains only FIFO writes and atomic stores — no allocation, no locks.
