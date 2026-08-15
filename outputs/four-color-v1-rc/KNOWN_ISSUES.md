# FOUR COLOR 1.0.0-rc.1 — known issues

Only real, open items are listed. Things that were deliberately decided are not
bugs and are not listed here as though they were.

---

## 1. ~~Windows RC binary does not exist yet~~ — RESOLVED

A real x86_64 VST3 now builds on GitHub's hosted Windows runners and is
published as a downloadable artefact on every push. The repository was made
public, which is what makes those minutes free; while it was private the
account's jobs were refused on billing and no push could produce a binary.

    473 checks, 0 failed, MSVC Release and Debug

The zip in dist/ holds the compiled plug-in, the standalone, and the installer.
Nothing has to be built by hand any more.

**Note for the future:** do not point the workflow at a self-hosted runner
while this repository is public. A pull request runs the workflow, and against
a runner on your own machine that is a stranger executing code on it.

## 2. Cubase has not been validated on either platform

**External blocker.** Cubase cannot be driven from this machine.

`CUBASE_TEST_HE.md` has 24 rows for macOS and 18 for Windows. **Every box is
empty** and none will be ticked from here.

This is the last thing standing between RC and FINAL.

---

## 3. pluginval has not been run

**Needs your approval.** It is not installed, and installing it means
downloading and running a third-party binary.

Official source: `https://github.com/Tracktion/pluginval/releases`

Intended run once approved: strictness level 10, against the VST3 and the AU.

auval already passes.

---

## 4. Signing and notarization are not done

**SIGNING/NOTARIZATION REQUIRED BEFORE PUBLIC RELEASE.**

The bundles carry an **ad-hoc** signature so they load on another Mac. That is
not notarisation. On a machine that has never seen the plug-in, the first
launch needs right-click → Open; `README-INSTALL-MAC.txt` explains it.

No certificate was invented and nothing unsigned is presented as shippable.

---

## 5. Owner listening approval is outstanding

140 files in `listening/`. `LISTENING_GUIDE_HE.md` ends with four questions
that no measurement can answer — chiefly whether the four engines actually read
as four different things, and whether BODY adds body or just volume.

**This blocks the preset expansion.** 31 presets ship in the RC; going to 60–80
before the character is settled would mean rebuilding them afterwards.

---

## 6. Residual visualisation — deferred to 1.1

`Analyzer::setResidualProvider()` is an empty seam. The data exists in
`HarmonicSpace`, the drawing does not.

This is a decision, not an omission: nothing is drawn that is not measured, so
while the provider is unwired the analyzer draws no residual layer and shows no
legend suggesting one exists.

---

## Things that are NOT bugs

Listed because they look like bugs and are not.

**A powered-off band still passes audio.** Power is not Mute. The frequency
range continues, clean — measured 0.002 dB from Drive 0. The analyzer keeps
drawing that band's spectrum and only drains its colour, because the audio is
genuinely still there and a hole would be a lie about the signal.

**The host calls it Bypass, the UI calls it Power, and the sense is inverted.**
`bN_bypass` is a frozen Parameter ID that 31 presets depend on. Power ON is
bypass FALSE. The tooltip reconciles the two names.

**A preset change measures 1.54× the settled slew.** Not a click. Sample by
sample it rises smoothly over about eight samples, 0.6 ms in: several smoothers
ramping together under the 15 ms colour crossfade. Every individual parameter
group, changed alone, measures 1.00–1.09×. The one that genuinely stepped —
band Tone at 3.43× — was found and fixed.

**BODY moves total RMS by less than 2 dB.** That was the old, wrong criterion.
BODY works by pushing a saturator, and a saturator compresses what it is fed,
so level is not what changes — density is. Measured as nonlinear residual, the
weakest source lifts 2.16 dB and the strongest 5.40 dB.

**1× oversampling still reports 65 samples of latency.** Every quality reports
the same figure so that Quality is safe to touch during playback. 1× pays for
latency it does not need; that is the price.
