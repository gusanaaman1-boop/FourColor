# FOUR COLOR — audition pack

```bash
cmake --build build --target FourColorRender -j 10
./build/FourColorRender_artefacts/Release/FourColorRender --audition audition
```

82 stereo WAVs, 4 s each, 48 kHz / 24-bit, about 90 MB. The folder is
`.gitignore`d — it is regenerated from the binary, not stored.

**Everything is loudness-matched to the same RMS and peak-limited below full
scale.** Comparing two saturators at different levels tells you which is louder,
not which is better, and a comparison that clips tells you nothing at all.

## What is in it

| Folder | Files | The question it answers |
| --- | --- | --- |
| `00-dry` | 6 | What the source sounds like untouched, at the same loudness |
| `01-colours` | 24 | The headline A/B: four colours, one drive, six sources |
| `02-drive` | 24 | Drive 20 / 50 / 80 on bass and drums |
| `03-behavior` | 12 | BODY / centre / ATTACK on drums, where the axis is the point |
| `04-space` | 16 | Space off against a musical amount, on pad and melody |

Sources: `bass`, `melody`, `drums`, `pad`, `vocal`, `full-loop`.

## About the source material

The six sources are generated in closed form inside the render tool, so two runs
are byte-identical and the pack carries no sample licences. They are deliberately
not one-shots: a rolling bass line over four roots, a pentatonic pluck figure,
kick/snare/hats at 120 bpm, four detuned pad voices, a sung vowel with vibrato
and three formants, and all of it summed.

They are a fair, repeatable A/B that isolates one variable at a time. They are
**not** a substitute for running your own material through the plug-in in Cubase,
and the decisions this pack is meant to inform should be confirmed there.

## What to listen for

1. **`01-colours` first.** Do WARM, IRON, BITE and FUZZ read as four different
   things, or as four amounts of the same thing? That is the product's whole
   premise. The measurements say the spectra differ by 7–28 dB, but a
   measurement cannot answer this question.
2. **`01-colours/bass-*` and `02-drive/bass-*` specifically.** Phase 16 made
   IRON's core-loss corner and BITE's pre-emphasis follow the band instead of
   sitting at 280 Hz and 1800 Hz. In the low band those constants used to do
   almost nothing. This is the first time IRON has density and BITE has bite
   below 120 Hz, and it is the change most likely to need a second opinion.
3. **`03-behavior`.** BODY should keep the hit clean and saturate the tail;
   ATTACK the reverse. Measured spread is 1.6–6.2 dB. Does it read as two
   behaviours or as two amounts of drive?
4. **`04-space`.** The halo should sound like it belongs to the distortion, not
   like a reverb sent from the source. If it sounds like a small room, that is a
   failure of the residual estimate, not a taste question.
5. **`02-drive/*-fuzz-d80`.** FUZZ is meant to be the broken one, but still
   usable. If it is unusable at 80, the range is wrong.

## The checkpoint

The brief stops here on purpose. Everything above this line was measured;
nothing above it was heard. Phase 16's remaining work — tuning the *character*
of the four engines — should not happen until you have listened and said what
you want changed.
