# FOUR COLOR — parameter map (IDs are frozen)

IDs are stable from Phase 1 on and must never change once presets exist.
All continuous audio-affecting parameters are smoothed in the engine.
Bands are indexed 0..3 = LOW, LMID, HMID, HIGH; per-band IDs use the prefix `b0_`..`b3_`.

## Global

| ID | Name | Type / range | Default | Notes |
| --- | --- | --- | --- | --- |
| `input` | Input | -24..+24 dB | 0 | input trim |
| `globalDrive` | Global Drive | 0..100 % | 50 | scales the four band drives around their values; 50 = neutral |
| `globalTone` | Global Tone | -100..+100 (Dark..Bright) | 0 | spectral tilt around 800 Hz |
| `autoLevel` | Auto Level | bool | on | slow bounded internal match, ±12 dB |
| `mix` | Mix | 0..100 % | 100 | latency-aligned dry/wet |
| `output` | Output | -24..+12 dB | 0 | output trim |
| `quality` | Quality | Draft/Normal/High/Ultra | High | 1x/2x/4x/8x oversampling |
| `bypassed` | Bypass | bool | off | latency-aligned global bypass |
| `xover1` | Crossover 1 | 40..400 Hz, log | 120 Hz | min ratio 1.30 to xover2 |
| `xover2` | Crossover 2 | 250..2500 Hz, log | 700 Hz | |
| `xover3` | Crossover 3 | 1500..12000 Hz, log | 4500 Hz | |

## Per band (×4, prefix `b<n>_`)

| ID suffix | Name | Type / range | Default | Notes |
| --- | --- | --- | --- | --- |
| `color` | Color | Warm/Iron/Bite/Fuzz | Warm | engine switch, short crossfade on change |
| `drive` | Drive | 0..100 % | 25 | |
| `behavior` | Behavior | -100..+100 (Body..Attack) | 0 | dynamic saturation axis |
| `tone` | Tone | -100..+100 (Dark..Bright) | 0 | band-scoped pre/post emphasis |
| `space` | Space | 0..100 % | 0 | harmonic-residual micro-diffusion |
| `bandMix` | Band Mix | 0..100 % | 100 | vs delay-aligned clean band |
| `level` | Band Level | -18..+12 dB | 0 | |
| `solo` | Solo | bool | off | |
| `mute` | Mute | bool | off | |
| `bypass` | Band Bypass | bool | off | passes the aligned clean band, not silence |

Total: 11 global + 4×10 per band = **51 host-visible parameters**.

UI-state saved as APVTS properties (not parameters): `selectedBand`, editor size.

## Smoothing / transition rules

- dB and % parameters: 20–50 ms linear/multiplicative smoothing.
- Crossover frequencies: smoothed, coefficients updated on a 32-sample control grid.
- `color`: 15 ms equal-power crossfade between the old and new engine output.
- `quality`: applied at block boundary via re-prepare; host is notified of the new latency.
- `solo`/`mute`/`bypass` (band and global): 10 ms gain fades, never hard switches.
