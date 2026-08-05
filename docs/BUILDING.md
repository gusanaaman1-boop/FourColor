# FOUR COLOR — building and validating

VST3 + AU + Standalone, JUCE 9, C++20. macOS is the development machine and runs Cubase 15;
**Windows 11 is the product target and has never been compiled or run** — see
[CUBASE-CHECKLIST.md](CUBASE-CHECKLIST.md).

## Pinned toolchain

Pinned so that a red build means "our code broke", not "something upstream moved".

| | macOS (verified) | Windows (not yet run) |
| --- | --- | --- |
| OS | macOS 26.3.2 | Windows 11 x64 |
| Compiler | Apple clang 21.0.0 (`clang-2100.1.1.101`) | MSVC v143 (Visual Studio 2022, 17.x) |
| CMake | 4.4.0 (Homebrew) | 3.22+ — the VS installer's copy is fine |
| Generator | Unix Makefiles | Visual Studio 17 2022, `-A x64` |
| Architectures | `arm64;x86_64` universal | x64 |
| JUCE | commit `857aab9c4eb3084af639a380a693dcec7d728b73` (JUCE 9, 2026-07-22) | same commit |
| CI image | — | `windows-2022` (pinned, not `windows-latest`) |

JUCE is expected at `~/JUCE` (macOS) or `%USERPROFILE%\JUCE` (Windows); CI checks it out
into `./JUCE`. `CMakeLists.txt` looks in all three.

## Build

```bash
scripts/build-macos.sh both --clean
```

```bat
scripts\build-windows.bat both --clean
```

Both scripts configure, build, and run the test suite, and both refuse to touch the system
plug-in folders unless you pass `--install`. Add `--install` only when you actually want
the plug-in registered on that machine.

Raw CMake, if you prefer:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFOURCOLOR_COPY_AFTER_BUILD=OFF -DFOURCOLOR_WERROR=ON
cmake --build build -j 10
./build/FourColorTests_artefacts/Release/FourColorTests
```

### Options

| Option | Default | Meaning |
| --- | --- | --- |
| `FOURCOLOR_COPY_AFTER_BUILD` | `ON` | Copy the built plug-ins into the user plug-in folders. The scripts default it to `OFF`. |
| `FOURCOLOR_WERROR` | `OFF` | Treat warnings **in FOUR COLOR's own sources** as errors. Both scripts and CI set it `ON`. |
| `FOURCOLOR_SANITIZERS` | `OFF` | Add the `FourColorTestsSan` target (ASan + UBSan). |

## Warning policy

JUCE's module sources compile into our targets, so a blanket `-Werror` on the target would
fail on third-party code. The flags are attached to FOUR COLOR's own translation units
only — that is what "zero warnings" honestly means here.

Enabled: `-Wall -Wextra -Wpedantic -Wshadow -Wcast-align -Wunused -Woverloaded-virtual
-Wnon-virtual-dtor -Wimplicit-fallthrough` (MSVC: `/W4 /permissive-`).

Deliberately **not** enabled: `-Wconversion`, `-Wsign-conversion`, `-Wdouble-promotion`,
`-Wfloat-equal`. Measured on this tree they produce 262 hits, essentially all intentional
float/int arithmetic in JUCE-idiomatic graphics and DSP code. Silencing them would mean
262 casts that change no behaviour and would bury the few that might matter.

## Sanitisers

```bash
cmake -B build-san -DCMAKE_BUILD_TYPE=Debug -DFOURCOLOR_SANITIZERS=ON
cmake --build build-san --target FourColorTestsSan -j 10
./build-san/FourColorTestsSan_artefacts/Debug/FourColorTestsSan
```

ASan + UBSan with `-fno-sanitize-recover=all`, so any finding is a non-zero exit. On MSVC
only ASan is available (`/fsanitize=address`); UBSan has no MSVC equivalent.

The suite is roughly 8x slower under sanitisers, which is why it is a separate opt-in
target and not the default test build.

## Test suite

`build/FourColorTests_artefacts/<config>/FourColorTests` — exit code 0 means every check
passed. Each check prints its measured value, so a failure is diagnosable without a
debugger. It replaces global `operator new` to prove the audio thread never allocates.

**Performance checks are Release-only.** In a Debug build the CPU numbers are printed but
not judged (`-- ... [performance check skipped: Debug build]`), because an unoptimised
build runs roughly 5x slower and lowering the Release thresholds to accommodate it would
be tuning a test to pass.

## pluginval — not installed

pluginval has **not** been installed or run. It is free and open source; installing it
needs your approval.

| | |
| --- | --- |
| Official source | <https://github.com/Tracktion/pluginval> — releases at <https://github.com/Tracktion/pluginval/releases> |
| Latest release | **v1.0.4** (as listed on the releases page) |
| Licence | GPLv3, by Tracktion Software Corporation |

Once installed, the run we want is strictness 10, which includes the parameter, state and
threading torture tests:

```bash
pluginval --strictness-level 10 --validate-in-process --output-dir outputs/pluginval \
          build/FourColor_artefacts/Release/VST3/FourColor.vst3
```

```bat
pluginval.exe --strictness-level 10 --output-dir outputs\pluginval ^
              build-win\FourColor_artefacts\Release\VST3\FourColor.vst3
```

Do not mark pluginval as passing anywhere until that command has actually been run and its
exit code was 0.

## auval (macOS only)

```bash
auval -v aufx Fclr Naam
```

Requires the AU to be installed in `~/Library/Audio/Plug-Ins/Components/`.
