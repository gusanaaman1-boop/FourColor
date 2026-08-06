# FOUR COLOR — end of the 30% round

**Status: not a Release Candidate, but Windows is now built and evidenced.**
GitHub Actions run 31030970761 compiled the plug-in with MSVC on windows-2022,
with /WX, in both Release and Debug, and both reported *369 checks, 1 failed* —
the same single known miss macOS reports. The artefact is a real x64 VST3.

This claim has a run ID behind it. An earlier version of this file asserted the
same thing from one sentence in chat and had to be withdrawn when an installer
search of the whole Windows user profile found no plug-in at all; that is the
difference between a report and a guess.

Three items of the brief remain outstanding and one acceptance criterion is
missed.

Head: `fbe9e95`. 369 checks, **1 failing** (deliberately — see §5).

---

## 1. Builds

| Target | Result |
| --- | --- |
| VST3 `build/FourColor_artefacts/Release/VST3/FourColor.vst3` | OK |
| AU `build/FourColor_artefacts/Release/AU/FourColor.component` | OK, `auval` PASS |
| Standalone `build/FourColor_artefacts/Release/Standalone/FourColor.app` | OK |
| Debug, all three | OK |
| ASan + UBSan test build | OK, 0 findings |
| Warnings from FOUR COLOR sources, `-Werror` on | **0**, Release and Debug |
| **Windows VST3 + Standalone** | **BUILT by CI**, MSVC x64, `/WX` clean. Run 31030970761. Release and Debug both 369 checks / 1 known failure. Not yet loaded in Cubase. |

macOS 26.3.2, Apple clang 21.0.0, CMake 4.4.0, JUCE `857aab9c`, universal
arm64 + x86_64.

## 2. Commits, in order

| Commit | Phase |
| --- | --- |
| `f393d50` | 10 — release baseline, warning policy, Windows + sanitiser scaffolding |
| `f9b596e` | 11 — Drive 0 clean pass-through; make-up gain measured correctly |
| `00fb2b9` | — corrected a false claim about Cubase in the docs |
| `9d0bab4` | 12 — one latency for every Quality; click-free Quality switching |
| `32cffdf` | — low-end review prompt + refreshed source appendices |
| `1c20cfa` | ColorContext plumbing (mislabelled "Phase 13"; see §7) |
| `6a65c5a` | Branding — Naaman monogram and maker's name |
| `7782ff2` | 13 — state versioning, migration, hostile input |
| `d661922` | 14 — Space estimator stays converged |
| `5de7207` | 15 — Behavior detector never stops watching |
| `e934218` | 16 — band-relative engine constants + audition pack |
| `d8dd992` | 17 + 19 — K-weighted Auto Level; analyzer cost down a quarter |
| `be2d417` | Windows quick-start |

## 3. What each phase actually changed

**10 — Baseline.** Clean from-scratch builds at zero warnings with `-Werror` on
FOUR COLOR's own translation units (not JUCE's, which compile into the same
targets). Fixed all 89 real warnings the strict set found; excluded
`-Wconversion` and friends after measuring 262 hits that were all intentional
float/int arithmetic. Performance checks became Release-only — a CPU number
from an unoptimised build is meaningless, and lowering the Release thresholds
to accommodate it would be tuning a test to pass.

**11 — Drive 0.** All four engines still shaped at Drive 0. They now fade out
completely over the first 5% of the range on a smoothstep, per-sample smoothed.
Static make-up moved inside the engine so unity is exact by construction.
`updateCompensation` had been integrating the **positive half cycle only** and
counting the DC an asymmetric curve makes as audible power; it now integrates
the full cycle with the mean removed.
*Worst THD+N at Drive 0 across 16 engine/band pairs: **−124.3 dB**. Worst gain
error: **0.0016 dB**.*

**12 — Latency.** Was 0/49/60/65 across the qualities, with the host told from
inside `processBlock`. Now **exactly 65 samples everywhere**, reported once in
`prepareToPlay`. The pad is applied in the *oversampled* domain, where it is an
exact integer for every factor — a first attempt padded at base rate, and the
fractional Thiran delay smeared transients enough to cost BODY/ATTACK 0.6 dB.
Quality switching runs two engine banks with a pre-roll and a 30 ms crossfade;
three destructive resets were removed.
*Mix 50% flatness improved from 0.13 dB to **0.0104 dB**. Pre-switch
bit-exactness: **0.000000000** on all materials.*

**13 — State.** `stateVersion` as a property, not a renaming scheme. Untagged
states load forever. Future states load and **keep what they do not
understand**. NaN, infinity and out-of-range values are dropped or clamped.
Seven golden fixtures.
*All 27 presets round-trip with **0** parameter mismatches.*

**14 — Space.** The least-squares fit only ran when Space was audible, so
turning the knob up started it cold and diffused the **clean source** for a
fifth of a second. Estimator now always runs; only the diffuser is skipped.
Added Tikhonov regularisation, an energy gate, and smoothed bounded
coefficients.
*At Drive 0 there is no residual, so a correct estimator diffuses nothing:
**−67.8 to −71.4 dB** leakage against a −30 dB budget. Tail at +300 ms:
**−96.9 dBFS**.*

**15 — Behavior.** The detector froze its envelopes at amount 0, so engaging it
started from whatever was playing when it was last touched. Now always running.
*A detector engaged halfway matches one running from the start to within
**−101 dBFS**. BODY→ATTACK monotonic on all four engines, spread 1.6–6.2 dB.*

**16 — Band-relative constants.** Three constants were tuned for one band and
applied to all four. IRON's core-loss at 280 Hz (the LOW MID centre) passed the
entire low band, so IRON had **no density below 120 Hz at all**. BITE's
emphasis at 1800 Hz (the HIGH MID centre) passed almost nothing down there, so
BITE collapsed to a bare diode. FUZZ's 0.5 ms gate rode the waveform of a 30 Hz
decay. All three now follow the band centre.
*Preset level impact: mean **0.025 dB**, worst 0.158 dB.*

**17 — Auto Level.** Raw RMS replaced with K-weighting (BS.1770's two stages,
designed at the actual sample rate), plus a miniature relative gate.
*Median loudness error **0.20 LU** (budget 0.35). Silence drift **0.0016 dB**
(budget 0.25).*

**19 — Analyzer.** Cost had crept to 15.2% of a core, over budget. Backdrop
cached to an image; spectrum paths built per band instead of one full-width
path drawn four times under four clips. **13.0–14.2%** now, no threshold moved.

## 4. Headline measurements

| | |
| --- | --- |
| Crossover recombination | flat to **0.0001 dB**, null at **−111.9 dB** |
| Reported latency | **65 samples**, identical in all four qualities |
| Global bypass alignment | 65 samples, error < 3e−8 |
| Mix 50% flatness | **0.0104 dB** worst across 8 frequencies |
| Drive 0 THD+N | **−124.3 dB** worst of 16 engine/band pairs |
| Aliasing, typical material at 4x | **−70.1 dB** |
| Engine separation | 7.1–27.6 dB log-spectral distance between pairs |
| Space switch-on leakage | **−67.8 dB** worst |
| BODY/ATTACK spread | 1.6–6.2 dB, monotonic |
| Auto Level median error | **0.20 LU** |
| CPU, stereo | **19.8x** realtime at 4x, **10.8x** at 8x |
| Analyzer | 14.2% of one core; whole editor 27.7% |
| Audio-thread allocations | **0** |
| Sanitiser findings | **0** |

## 5. The criterion that is missed

`worst loudness error is under 0.75 LU` **fails at 0.86**, on the melody
source, and the check is left red.

Off, melody's error is 4.20 LU; on, 0.86 — so 78% is corrected and the last
fifth is not. I could not find the cause. A relative gate did not move it, a 2 s
power window made it slightly worse, and the safety stage only clamps above
+12 dBFS so it is not clipping. Five of the six sources pass comfortably
(0.04–0.48 LU).

It is worth more as a red check than as a quietly relaxed threshold.

## 6. What is not done

1. **Phase 18 — residual visualisation.** Not started.
   `Analyzer::setResidualProvider()` is still an unwired seam. `HarmonicSpace`
   now computes a real residual, so the data exists; nothing exposes it to the
   GUI. Deliberately not faked.
2. **Phase 20 — preset expansion.** Still **27** presets, not the 60–80 asked
   for. Writing 40 musical presets is authoring work that should happen after
   you have heard the audition pack, not before — the engines' low-band
   character changed in Phase 16.
3. **Phase 16 — character tuning.** The audition pack exists; the listening has
   not happened. This is the checkpoint the brief asked for.
4. **Windows and Cubase.** Windows now BUILDS and RUNS (2026-08-05) — Naaman
   built the source bundle on his own machine and reports it working well. The
   detail behind that has not been captured yet: which rows of the Cubase
   checklist were exercised, what the test suite printed there, and what
   latency Cubase reports. Until those are filled in, the Windows column of
   [CUBASE-CHECKLIST.md](CUBASE-CHECKLIST.md) stays open.

## 7. Two things about the record

**A numbering collision.** Commit `1c20cfa` labelled itself "Phase 13" for the
ColorContext plumbing. The brief's Phase 13 is state versioning. That commit
was also made from a different chat session by mistake — a FOUR COLOR
instruction pasted into the DigiMeter session. The code is correct and in the
right repository and its tests pass; only the label was wrong, and the test
section has been renamed.

**A correction to my own earlier reporting.** I wrote in the docs that no
machine in this workspace runs Cubase. That was wrong — Cubase 15 is installed
on this Mac. The real gap is Windows. Fixed in `00fb2b9`; the checklist now has
separate macOS and Windows columns, because a green macOS column does not
license a Windows claim.

## 8. Next, in order

1. ~~Build on Windows.~~ **Done, 2026-08-05.** It builds and runs.
2. **Work the Cubase checklist**, Windows column — the rows, not the
   impression. Latency reading, save/reopen, offline export null, and what the
   test suite printed on Windows are the four that matter most.
3. **Listen to the audition pack** — [AUDITION.md](AUDITION.md) says what for.
   Start with `01-colours/bass-*`, which is where Phase 16 changed most.
4. Then: preset expansion, residual visualisation, and whatever the listening
   and the Windows run turn up.
