# FOUR COLOR on Windows — first run

**Nothing in this file has been executed.** No machine in this workspace runs
Windows, so the build script and the CI workflow are written but unproven. The
first real run will find something; that is what it is for.

## What you need

| | |
| --- | --- |
| Visual Studio 2022 | with the **"Desktop development with C++"** workload (MSVC v143, x64) |
| CMake 3.22+ | the copy in the VS installer is fine |
| JUCE 9 | at `%USERPROFILE%\JUCE`, at the exact commit below |
| Git | to fetch both |

## 1. Get JUCE at the pinned commit

The Mac builds against one specific JUCE commit. Use the same one, or a
difference in JUCE becomes a difference in the plug-in that nobody can explain.

```bat
git clone https://github.com/juce-framework/JUCE.git "%USERPROFILE%\JUCE"
cd /d "%USERPROFILE%\JUCE"
git checkout 857aab9c4eb3084af639a380a693dcec7d728b73
```

## 2. Get FOUR COLOR onto the Windows machine

The repository has no remote — it lives only on the Mac. Either copy the folder
across, or add a remote and push. What you need is the whole `FourColor`
directory **except** `build*`, `audition` and `outputs`, which are all
regenerated.

## 3. Build and test

```bat
cd /d <wherever you put it>\FourColor
scripts\build-windows.bat both --clean
```

That configures, builds Release and Debug, and runs the full test suite for
each. It does not touch your VST3 folder unless you add `--install`.

**Expect the suite to report one failure.** `worst loudness error is under
0.75 LU` fails at 0.86 on the melody source — a known, documented miss, not a
Windows problem. Everything else should be green. If anything *else* fails,
that is genuinely new information, because every one of those checks passes on
macOS.

Each check prints its measured value, so a failing line tells you the number
and the budget without needing a debugger.

## 4. Install for Cubase

```bat
xcopy /E /I /Y "build-win\FourColor_artefacts\Release\VST3\FourColor.vst3" ^
      "C:\Program Files\Common Files\VST3\FourColor.vst3"
```

Needs an **administrator** Command Prompt. Copy the `.vst3` **folder**, not its
contents — a VST3 on Windows is a directory bundle.

## 5. What to check first in Cubase

[CUBASE-CHECKLIST.md](CUBASE-CHECKLIST.md) is the full list, in eleven sections.
If you only have twenty minutes, do these, in this order — they are the ones
most likely to expose a platform difference:

1. **Scan.** It appears under Naaman, no blocklist entry, no duplicate on rescan.
2. **Open the editor.** 980×620, all controls drawn, no missing text. Windows
   picks different fonts than macOS and the layout has never been seen there.
3. **Reported latency is 65 samples**, at every Quality setting. This is the
   number most likely to be wrong if MSVC's oversampler differs.
4. **Play something and move a crossover handle.** No zipper, no click.
5. **Switch Quality during playback**, all four. No dropout.
6. **Save the project, close Cubase, reopen.** Every parameter returns.
7. **Offline export**, then compare against the real-time render. They should
   null.

Record what you find against the Windows column of the checklist. A row that
passes on macOS and fails on Windows is exactly the thing this whole exercise
exists to catch.

## If the build fails

| Symptom | Almost always |
| --- | --- |
| `cmake is not on PATH` | Use the "Developer Command Prompt for VS 2022" |
| CMake cannot find a compiler | The C++ workload is not installed |
| Errors inside `JUCE\modules\...` | JUCE is at the wrong commit |
| Errors in `Source\...` with `/WX` | A real MSVC-only warning. Send me the line — this is the most useful thing the Windows build can tell us |
