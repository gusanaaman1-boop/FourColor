@echo off
REM FOUR COLOR - Windows 11 / MSVC x64 build.
REM Mirrors scripts/build-macos.sh step for step.
REM
REM   scripts\build-windows.bat [Release^|Debug^|both] [--clean] [--install]
REM
REM Prerequisites (see docs/BUILDING.md for the pinned versions):
REM   * Visual Studio 2022, "Desktop development with C++", MSVC v143 x64
REM   * CMake 3.22 or newer (the VS installer's copy is fine)
REM   * A JUCE 9 checkout at %USERPROFILE%\JUCE
REM
REM Run from a normal Command Prompt; CMake finds the VS toolchain itself.
REM Installing into the system VST3 folder is opt-in because it needs
REM administrator rights.

setlocal enabledelayedexpansion
cd /d "%~dp0.."

set CONFIGS=Release
set DO_CLEAN=0
set DO_INSTALL=OFF

:parse
if "%~1"=="" goto parsed
if /I "%~1"=="Release" set CONFIGS=Release
if /I "%~1"=="Debug"   set CONFIGS=Debug
if /I "%~1"=="both"    set CONFIGS=Release Debug
if /I "%~1"=="--clean" set DO_CLEAN=1
if /I "%~1"=="--install" set DO_INSTALL=ON
shift
goto parse
:parsed

if not exist "%USERPROFILE%\JUCE\CMakeLists.txt" (
    echo ERROR: no JUCE checkout at %USERPROFILE%\JUCE
    echo        git clone --branch master --depth 1 https://github.com/juce-framework/JUCE.git "%USERPROFILE%\JUCE"
    exit /b 1
)

for %%C in (%CONFIGS%) do (
    set BUILD_DIR=build-win
    if /I "%%C"=="Debug" set BUILD_DIR=build-win-debug

    if "%DO_CLEAN%"=="1" (
        echo == removing !BUILD_DIR!
        if exist "!BUILD_DIR!" rmdir /s /q "!BUILD_DIR!"
    )

    echo == configure %%C -^> !BUILD_DIR!
    REM Multi-config generator: CMAKE_BUILD_TYPE is ignored, --config decides.
    cmake -B "!BUILD_DIR!" -G "Visual Studio 17 2022" -A x64 ^
          -DFOURCOLOR_COPY_AFTER_BUILD=%DO_INSTALL% ^
          -DFOURCOLOR_WERROR=ON
    if errorlevel 1 exit /b 1

    echo == build %%C
    cmake --build "!BUILD_DIR!" --config %%C --parallel
    if errorlevel 1 exit /b 1

    echo == tests %%C
    "!BUILD_DIR!\FourColorTests_artefacts\%%C\FourColorTests.exe"
    if errorlevel 1 (
        echo TESTS FAILED in %%C
        exit /b 1
    )
)

echo.
echo Artefacts:
if exist "build-win\FourColor_artefacts\Release\VST3\FourColor.vst3" ^
    echo   build-win\FourColor_artefacts\Release\VST3\FourColor.vst3
if exist "build-win\FourColor_artefacts\Release\Standalone\FourColor.exe" ^
    echo   build-win\FourColor_artefacts\Release\Standalone\FourColor.exe
echo.
echo Install by hand (needs admin):
echo   copy the .vst3 folder into "C:\Program Files\Common Files\VST3\"
endlocal
