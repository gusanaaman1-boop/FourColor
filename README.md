# FOUR COLOR

A four-band colour/saturation processor (VST3 / AU / Standalone, JUCE 9, C++20) — a
modern spiritual successor to the idea of a four-band fuzz. Not a port of anyone's code,
UI, names or artwork.

Four bands · four colour engines (WARM / IRON / BITE / FUZZ) · a BODY↔ATTACK dynamic
Behavior axis per band · HARMONIC SPACE that diffuses only what the saturation created ·
oversampling up to 8x with a strict latency contract · slow bounded Auto Level.

**Status: 70% stage complete.** See [docs/REPORT-70.md](docs/REPORT-70.md) for what is
done, measured and still open, [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the DSP
design, and [docs/PARAMETERS.md](docs/PARAMETERS.md) for the frozen parameter map.

## Build

Requires CMake ≥ 3.22 and the shared JUCE 9 checkout at `~/JUCE` (workspace convention).

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Targets: `FourColor_VST3`, `FourColor_AU` (macOS), `FourColor_Standalone`,
`FourColorTests` (measurement suite, exit 0 = all pass), `FourColorShot`
(deterministic UI screenshots into `ui-shots/`).

`-DFOURCOLOR_COPY_AFTER_BUILD=ON` (default) installs plug-ins into the user plug-in
folders after building.

## Tests

```
./build/FourColorTests_artefacts/Release/FourColorTests
```

290 checks: crossover recombination/null, engine harmonic distinctness, aliasing by
quality, behavior, space residual isolation, latency alignment, comb-free mix, presets,
CPU, and an allocation tripwire proving the audio thread never allocates.

Company: Naaman · Plugin code: `Fclr` · Formats: VST3, AU, Standalone
