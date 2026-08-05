# FOUR COLOR — questions from the implementing engineer

**5 August 2026 · after baseline + Phase 13 · `1c20cfa`**

Baseline verified: **321 checks, 0 failed**; VST3, AU and Standalone all build;
the two appendices match HEAD exactly (22 files, 0 differences).

**Phase 13 is committed and green: 330 checks, 0 failed, Release and Debug.**

Five things need a decision before or during the phases that follow. Four are
genuine forks in the directive; one is a request for permission.

---

## 1. REQUEST — a flaky performance test is about to hide real regressions

`analyzer repaint stays under 15% of one core` **fails intermittently on
unchanged code.** Measured five times each on this machine:

| | range across 5 runs | failed |
|---|---|---|
| **Baseline, Phase 13 stashed** | 14.8 – 15.2% | **3 of 5** |
| With Phase 13 applied | 14.8 – 15.3% | **3 of 5** |

Identical distributions, and Phase 13 touched no UI file. The check straddles
its own threshold and decides by machine noise.

Rule 9 says run every test after every phase. Rule 10 says never lower a
threshold to make a test pass. Both are right — but with one test failing ~60%
of the time at random, **I cannot tell a real regression from noise for the
eight phases still to come.**

**Request:** permission to change the *measurement* only — take the fastest of
N short samples instead of one long one — while leaving the **15% threshold
exactly as it is**. The fastest sample is the one least contaminated by other
work on the machine, and it cannot flatter a genuinely slow renderer.

**This is not lowering a threshold.** If it still reads as too close to rule 10,
say so and I will leave the test alone and report its state honestly after every
phase instead.

---

## 2. Phase 14.2 — the BITE brief contains a real internal conflict

The prescribed formula, applied to the actual band edges:

| band | range | centre | `biteEmphasisHz` |
|---|---|---|---|
| **LOW** | 20–120 Hz | 49 Hz | **76.7 Hz** |
| LOW MID | 120–700 | 290 | 450 |
| HIGH MID | 700–4500 | 1775 | 2826 |
| HIGH | 4500–16000 | 8485 | 8000 (clamped) |

The directive also says, in the same section:

- keep the **partial** de-emphasis, and
- the **30–60 Hz fundamental must not move more than 1 dB** beyond planned
  compensation.

Those pull against each other. Pre-emphasis at 76.7 Hz attenuates 30–60 Hz going
into the shaper; if the de-emphasis is only partial, that attenuation is by
definition not fully undone. Partial de-emphasis is exactly what makes BITE
sound forward, so making it complete in LOW changes the colour's identity there.

**Question:** in LOW specifically, which wins?

- **(a)** Keep the partial de-emphasis. BITE stays BITE; accept that the
  fundamental moves and widen that acceptance criterion for LOW only.
- **(b)** Make the de-emphasis complete in LOW only. The fundamental is
  protected; BITE in the sub is a different character from BITE elsewhere.
- **(c)** Compensate the fundamental with a measured static gain so both
  criteria are met, accepting that the compensation is tuned, not derived.

I will implement **(a)** and report the measured fundamental shift if no answer
arrives — it preserves the engine's identity, which the brief treats as the
higher goal ("השיניים של BITE ו־FUZZ חייבות להישאר זמינות").

---

## 3. Phase 18 — the render matrix as written is 2,160 files

10 sources × 4 colours × 3 drives × 3 behaviors × 2 Space × 3 crossovers.
The directive also says not to produce unnecessary duplicates, without saying
which axes to collapse.

**Proposed rule, which I will follow unless told otherwise:**

| source group | colours | drive | behavior | Space | crossover |
|---|---|---|---|---|---|
| sub sine, 808, rolling bass, kick | all 4 | 20/50/80 | BODY/N/ATTACK | 0/30 | 80/120/180 |
| drum loop, full electronic loop | all 4 | 50 | BODY/ATTACK | 0/30 | 120 |
| pluck, lead, pad, vocal | all 4 | 50 | Neutral | 0 | 120 |

≈ **310 files** rather than 2,160, with the full sweep kept exactly where the
directive says the problem is — the low end — and the melodic material reduced to
what actually distinguishes the four colours.

**Question:** accept this, or specify a different reduction?

---

## 4. Phase 21 — `pluginval` is not installed on this machine

Confirmed absent: not on `PATH`, not in `/Applications`. The directive says
report it and do not install without approval. **Reporting it.**

pluginval is free and open source (Tracktion). **Question:** install it, or ship
the Test Candidate with `auval` only and mark pluginval as not run?

---

## 5. Installation — two facts the directive should know

- **An AU is already installed** at
  `~/Library/Audio/Plug-Ins/Components/FourColor.component`, and the VST3
  directory exists. Both will be backed up with a timestamp before anything is
  replaced, per rule 14.
- **Cubase 15 is installed** at `/Applications/Cubase 15.app`, and **has still
  never been opened with this plug-in.** Nothing in this round changes that: I
  can install, but I cannot claim a Cubase result I have not seen. The status
  line will read `CUBASE macOS NOT VERIFIED` until the user reports back.

Windows has never been compiled here. It will be marked
`WINDOWS NOT VERIFIED` / `CUBASE WINDOWS NOT VERIFIED` unless a CI run is
actually executed and observed.

---

## WHAT I AM DOING WHILE WAITING

Not blocking on any of this. Proceeding in order — Phase 14 (DC blocker, BITE
emphasis, FUZZ gate, stereo linking, and the measurement-only IRON/Tone study),
then 15, 16, 17 — with a separate commit per phase and a full Release+Debug test
run after each.

The four questions above only change **how** a phase is finished, not whether it
starts. Question 1 changes how much I can trust the verification of all of them.
