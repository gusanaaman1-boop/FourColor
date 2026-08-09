# FOUR COLOR — Quick Start

**Multiband Colour & Saturation** · by Gussa Naaman · v1.0.0-rc.1
VST3 · AU · Standalone

---

## What it does

FOUR COLOR splits the signal into **four bands** and gives each one **its own
colour engine**.

It is not an EQ and not a compressor. It is **colour** — harmonics added
differently in each region. Bass can take weight and density while the top
takes bite, without either working against the other.

---

## The four colours

| | |
| --- | --- |
| **WARM** | Soft saturation with programme-dependent sag. Round and warm. |
| **IRON** | Feedback loop with a core-loss term. Density and weight. |
| **BITE** | Asymmetric diode pair with pre- and de-emphasis. Forward and cutting. |
| **FUZZ** | Partial rectification, double fold and a gate. Broken on purpose. |

They are not four settings of one thing — they are **four different
structures**.

---

## The four bands

**LOW · LOW MID · HIGH MID · HIGH**

Default crossovers: 120 Hz · 700 Hz · 4.5 kHz. Drag the lines in the analyzer.

A 4th-order Linkwitz-Riley tree with sibling allpass compensation. It is
**flat by construction**, not by calibration — the four bands recombine to
within 0.0001 dB.

---

## The controls

### DRIVE
How hard the engine is pushed. **At 0 the band is genuinely clean** — not
nearly clean: THD+N of −124 dB and a gain error of 0.0016 dB.

### SHAPE — BODY ↔ ATTACK
The most important axis in the plug-in.

- **ATTACK (right):** raises drive on the transient. Crunch and presence on
  the hit.
- **BODY (left):** adds **density** to the body and the tail, and leaves the
  hit alone.

BODY works through two mechanisms: it raises pre-gain *and* reinforces the
non-linear product itself. The reason is physical — once a signal is already
saturated, more drive generates almost no new harmonics, but what is already
there can still be made more audible.

**Note:** BODY adds density, not volume. If you want more decibels, that is
LEVEL.

### TONE — DARK ↔ BRIGHT
±9 dB of tilt **around the band's own centre**. This is not an EQ after the
engine: half the tilt happens **before** the shaper, so it changes *what* is
distorted, not just the colour of the result.

### SPACE / SPREAD
Diffuses **only the non-linear product**, never the clean signal. It widens
without hollowing the centre. **The low band stays mono** at every setting.

### MIX (per band) and MASTER MIX
Parallel processing. The dry leg is phase-aligned, so there is no comb
filtering — at 50 % it is flat to 0.0104 dB.

### ⏻ S M — at the top of each band

| | |
| --- | --- |
| **⏻ Power** | The band passes **clean**. Still audible. **Not a mute.** |
| **S Solo** | Only soloed bands are heard. |
| **M Mute** | The band is silenced. Mute always wins. |

Your host calls this parameter "Bypass" — same control, inverted sense.

### INPUT / OUTPUT meters
Either side of the analyzer, each with its trim underneath. The left meter
reads **after** the Input Trim. **CLIP** latches at and above 0 dBFS; click it
to reset.

### AUTO LEVEL
Matches the perceived loudness of the output to the input. **K-weighted, per
BS.1770** — not raw RMS, because RMS treats 40 Hz and 3 kHz as equally loud,
which is exactly the mistake that makes a saturator seem to change level when
it has only changed spectrum.

Slow, bounded to ±12 dB, and it never chases a noise floor.

### QUALITY
Draft 1× · Normal 2× · **High 4×** · Ultra 8×.

**Latency is identical at all four — 65 samples** — so it is safe to change
during playback without your host re-aligning anything.

> **Draft is for fast work, not for export.** At 1× there is no anti-aliasing
> headroom. Switch to High or Ultra before a final render.

### MASTER
Opens a drawer with **GLOBAL DRIVE** and **GLOBAL TONE**, which scale all four
bands together. Automation on them keeps working while the drawer is closed.

---

## Gain staging, briefly

1. **Come in around −18 dBFS.** The engines are calibrated for it.
2. **Use Input Trim rather than the channel fader**, so your A/B stays fair.
3. **Turn Auto Level on** while you search, off once you have committed to a
   level.
4. **Check the Output** before you print. CLIP lights at 0 dBFS.

---

## Three things to try now

1. **Bass:** LOW band, WARM, drive 55, SHAPE fully left. The tail thickens and
   the attack does not move.
2. **Kick:** LOW band, IRON, drive 60, SHAPE fully right. The hit gains
   presence.
3. **Full mix:** drive 30 on all four bands, Auto Level on, MASTER MIX at 40 %.
   Cycle the four colours and listen to the difference.

---

**Installed at:**
`~/Library/Audio/Plug-Ins/VST3/FourColor.vst3`
`~/Library/Audio/Plug-Ins/Components/FourColor.component`

In Cubase: under **Naaman**, category **Distortion**.
