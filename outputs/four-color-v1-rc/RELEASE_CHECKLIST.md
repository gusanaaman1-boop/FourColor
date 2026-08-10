# FOUR COLOR 1.0.0-rc.1 — release checklist

Ticked only where the step was actually performed. Anything not run is left
empty, not assumed.

---

## Done

- [x] 457 checks, 0 failed — Release
- [x] 457 checks, 0 failed — Debug
- [x] 457 checks, 0 failed — ASan + UBSan, 0 sanitizer findings
- [x] The Windows package builds and passes from a clean extraction
- [x] Sample-rate × block-size matrix: 6 rates × 9 block sizes × mono/stereo
- [x] 0 audio-thread allocations across 100 randomised sessions
- [x] 0 NaN / Inf
- [x] Latency is 65 samples at every quality
- [x] Crossover nulls at −111.9 dB
- [x] Drive 0 THD+N −124.3 dB, gain error 0.0016 dB
- [x] Mix 50 % flat to 0.0104 dB
- [x] Space leakage −66.8 to −71.4 dB; LOW band mono bit-exact
- [x] Meters calibrated: peak 0.0135 dB, RMS 0.0005 dB
- [x] All 16 Power masks reconstruct cleanly
- [x] 31 presets: load, unique, round-trip, safe peak, coherent Power state
- [x] No single parameter steps on preset load
- [x] auval PASS
- [x] macOS universal VST3 + AU + Standalone (x86_64 + arm64, lipo-verified)
- [x] Version 1.0.0-rc.1 consistent across code, packaging and UI
- [x] Bundle identifiers and manufacturer code consistent
- [x] `stateVersion` unchanged, migration chain intact
- [x] Previous install backed up with a timestamp, nothing deleted
- [x] RC installed locally; installed binaries hash identical to built
- [x] Cubase cache untouched; no user project or preset modified
- [x] macOS installer package built — two files, one of them the installer
- [x] Hashes recorded
- [x] Screenshots captured
- [x] Listening pack: 140 files
- [x] Delivery documents written

---

## Blocked — external, not work I can do

- [ ] **Windows RC binary.** No Windows machine, no cross-compiler, hosted
      GitHub Actions unavailable on this account since 2026-08-06. Build with
      `scripts\build-windows.bat Release`.
- [ ] **Windows CI green.** Same cause.
- [ ] **pluginval, strictness 10.** Not installed; installing a third-party
      binary needs your approval. Source:
      `https://github.com/Tracktion/pluginval/releases`
- [ ] **Code signing.** No credentials. Ad-hoc signature only.
- [ ] **Notarization.** No credentials.

---

## Owner validation — only you can do these

- [ ] **LISTENING: PASS** — `LISTENING_GUIDE_HE.md`, four questions at the end
- [ ] **CUBASE MACOS: PASS** — `CUBASE_TEST_HE.md`, 24 rows
- [ ] **CUBASE WINDOWS: PASS** — same document, 18 rows
- [ ] **INSTALLER CLEAN MACHINE: PASS** — a Mac that has never seen the plug-in

---

## The gate

**FOUR COLOR v1.0 RELEASE CANDIDATE READY FOR OWNER VALIDATION** — reached.

**FOUR COLOR v1.0 FINAL** — requires all four owner lines above returning PASS,
plus:

```
PLUGINVAL:     PASS
AUVAL:         PASS   (already)
SIGNING:       PASS
NOTARIZATION:  PASS
0 RELEASE BLOCKERS
```

---

## After FINAL, not before

- Expand 31 presets to 60–80. Blocked on the listening pass: BODY's character
  changed in this round, and building 70 presets on a character that may still
  move means building them twice.
- Residual visualisation (1.1). The seam is in place and empty.
