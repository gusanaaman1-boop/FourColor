# FOUR COLOR — Cubase 15 host validation checklist

**STATUS: WINDOWS/CUBASE NOT VERIFIED.**
No machine in this workspace runs Windows or Cubase. Nothing in this document has been
executed. Every row below is `[ ]` until somebody runs it on the target host and fills in
the result, the date and the build hash. Do not mark a row `PASS` from a macOS run.

Target: **Cubase 15, Windows 11 x64**, VST3 built by `scripts\build-windows.bat Release`.

| Field | Value |
| --- | --- |
| Build hash | _fill in_ |
| Cubase version | _fill in_ |
| Windows build | _fill in_ |
| Audio device / driver | _fill in_ |
| Tester / date | _fill in_ |

---

## 1. Scan

- [ ] **1.1** Copy `FourColor.vst3` into `C:\Program Files\Common Files\VST3\`.
- [ ] **1.2** Cubase → Studio → VST Plug-in Manager → Update. FOUR COLOR appears under
      Naaman, category **Distortion**.
- [ ] **1.3** No entry in the blocklist, no "failed to load" dialog.
- [ ] **1.4** Reported I/O is **stereo in / stereo out**, and mono→mono also instantiates.
- [ ] **1.5** Rescan a second time: no duplicate entry, no changed UID.

## 2. Instantiate

- [ ] **2.1** Insert on an audio track. Editor opens at 980×620.
- [ ] **2.2** Insert on a group/FX channel and on the stereo out. All three open.
- [ ] **2.3** Open four instances at once. No crash, no shared-state bleed between them.
- [ ] **2.4** Close and reopen the editor ten times. No leak dialog, no crash on close —
      this is where a mis-ordered attachment usually shows up.
- [ ] **2.5** Resize to the 900×560 minimum and to 1400×900. Layout holds, no clipped text.

## 3. Playback

- [ ] **3.1** Play a full mix. No dropouts at the project buffer size.
- [ ] **3.2** Reported latency in Cubase matches **65 samples** at every Quality setting.
- [ ] **3.3** Solo/mute a band during playback: no click, no level jump.
- [ ] **3.4** Move each of the three crossover handles during playback: smooth, no zipper.
- [ ] **3.5** Switch Colour on a band during playback: crossfade only, no discontinuity.
- [ ] **3.6** Leave it playing for 10 minutes. CPU stays flat; no creeping rise.

## 4. Automation

- [ ] **4.1** All 51 parameters appear in Cubase's automation list with readable names.
- [ ] **4.2** Write and play back automation for `b1_drive`, `b3_behavior`, `mix`,
      `xover2`. Curves are followed, no stepping.
- [ ] **4.3** Fast automation (a full sweep in under 100 ms) on Drive: no click.
- [ ] **4.4** Touch/latch modes behave; parameter gestures begin and end correctly
      (a knob drag writes one continuous curve, not a staircase).
- [ ] **4.5** `quality` automation is **not** expected to be smooth — it is a
      structural switch. Confirm it does not crash; document any audible seam.

## 5. Project save / reopen

- [ ] **5.1** Set a non-default state on four instances, save, close Cubase, reopen.
      Every parameter returns bit-identical.
- [ ] **5.2** Selected band, editor size and Quality survive the round trip.
- [ ] **5.3** Save a project made with an **earlier** build, open with the current build:
      loads without resetting to defaults (this is what the state versioning is for).
- [ ] **5.4** "Save As" a copy, reopen the copy. Same result.

## 6. Preset recall

- [ ] **6.1** All 27 factory presets load from the plug-in's own menu.
- [ ] **6.2** Cubase's own preset browser lists and loads them.
- [ ] **6.3** Save a user preset from Cubase, reload it in a new instance.
- [ ] **6.4** Switch presets during playback: no click, no runaway level.
- [ ] **6.5** The modified marker `*` appears after an edit and clears on preset load.

## 7. Sample-rate change

- [ ] **7.1** 44.1 → 48 → 88.2 → 96 → 192 kHz with the plug-in loaded and playing.
- [ ] **7.2** No crash, no stuck state, no NaN (listen for silence or full-scale noise).
- [ ] **7.3** Reported latency after each change is still 65 samples.
- [ ] **7.4** Crossover frequencies still land where the display says they do.

## 8. Buffer-size change

- [ ] **8.1** 32, 64, 128, 256, 512, 1024, 2048 samples.
- [ ] **8.2** Change buffer size mid-playback: no crash, no persistent glitch.
- [ ] **8.3** At 32 samples with Quality = ULTRA 8x, note the CPU figure.

## 9. Offline export

- [ ] **9.1** Export the mix in real time and offline. The two files **null** against each
      other (import both, invert one; residual below −90 dBFS).
- [ ] **9.2** Export at 2x and 4x offline speed: identical result.
- [ ] **9.3** The first 65 samples are not shifted — PDC applied on export too.
- [ ] **9.4** Export with Auto Level on: no gain ramp at the file start.

## 10. Bypass

- [ ] **10.1** Cubase's own insert bypass: click-free, and the bypassed signal is
      **sample-aligned** with the processed one (null test against the raw track).
- [ ] **10.2** The plug-in's own Power button: same test.
- [ ] **10.3** Toggle both rapidly during playback: no click, no latency jump.

## 11. Quality switch

- [ ] **11.1** DRAFT → NORMAL → HIGH → ULTRA and back, during playback.
- [ ] **11.2** No dropout longer than one sample, no click.
- [ ] **11.3** Reported latency does **not** change (65 samples throughout).
- [ ] **11.4** CPU scales as expected; ULTRA is usable at the project buffer size.

---

## Reporting a failure

For each failure record: the row number, what was heard or seen, the Cubase version, the
sample rate and buffer size, whether it reproduces in a fresh project, and — if it crashes
— the Windows crash dump path. A failure that only shows up at one buffer size is still a
failure; note the size.
