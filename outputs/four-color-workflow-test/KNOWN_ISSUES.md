# FOUR COLOR — known issues, workflow test candidate

Only unresolved things. Everything else is in TEST_REPORT.md.

## 1. BODY reaches the 2 dB target on one source of four

| source | decay lift |
| --- | --- |
| bass note | **+2.74 dB** |
| 808 | +1.70 dB |
| pad | +1.15 dB |
| vocal sustain | +0.57 dB |

The brief asks for at least 2 dB on all four. The depth cap it also sets is
6 dB of pre-gain, and 6 dB into a saturator does not come out as 6 dB — the
curve compresses it. I implemented to the cap rather than exceeding a stated
maximum to make a number go green, so the check is red.

Raising `maxBodyDb` past 6 would close it. That is a one-line change and your
call, not mine.

Attack window and silence are both clean: +0.00 dB on the initial 20 ms for
every source, silent tail at −200 dBFS, and a −66 dBFS noise floor rises
0.000 dB.

## 2. One loudness criterion, carried from the previous round

`worst loudness error is under 0.75 LU` fails at **0.86** on the melody source.
Off it is 4.20 LU, on it is 0.86, so 78% is corrected and the rest is
unexplained. Five of six sources pass at 0.04–0.48 LU. Documented in
REPORT-RC.md section 5.

## 3. Cubase has not been run

Not on macOS, not on Windows. The Windows x64 build exists (CI run
31030970761) and this macOS build is installed, but no row of
docs/CUBASE-CHECKLIST.md has been executed. That is what the test guide is for.

## 4. pluginval not installed

Free, GPLv3, github.com/Tracktion/pluginval. Not installed, so not run, so not
claimed. The command is in docs/BUILDING.md.

## 5. Windows build predates this round

CI run 31030970761 was built before the workflow changes. Push to the repo and
the Windows build refreshes itself; the artefact is a ready-to-install folder.

## 6. Phase 18 from the previous brief is still not started

`Analyzer::setResidualProvider()` remains an unwired seam. HarmonicSpace
computes a real residual; nothing exposes it to the GUI. Deliberately not
faked.
