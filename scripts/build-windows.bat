@echo off
REM FOUR COLOR - Windows 11 / MSVC x64 build.
REM
REM   scripts\build-windows.bat [Release^|Debug^|both] [--clean] [--install]
REM
REM Prerequisites (docs\BUILDING.md has the pinned versions):
REM   * Visual Studio 2022, "Desktop development with C++", MSVC v143 x64
REM   * CMake 3.22 or newer - the one in the VS installer is fine
REM   * A JUCE 9 checkout at %USERPROFILE%\JUCE
REM
REM Run from a normal Command Prompt; CMake finds the VS toolchain itself.
REM Visual Studio is a MULTI-CONFIG generator, so one build directory holds
REM both Release and Debug - only --config changes.

setlocal enabledelayedexpansion
cd /d "%~dp0.."

set CONFIGS=Release
set DO_CLEAN=0
set DO_INSTALL=OFF
set BUILD_DIR=build-win

:parse
if "%~1"=="" goto parsed
if /I "%~1"=="Release"   set CONFIGS=Release
if /I "%~1"=="Debug"     set CONFIGS=Debug
if /I "%~1"=="both"      set CONFIGS=Release Debug
if /I "%~1"=="--clean"   set DO_CLEAN=1
if /I "%~1"=="--install" set DO_INSTALL=ON
shift
goto parse
:parsed

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: cmake is not on PATH.
    echo        Install CMake, or use the "Developer Command Prompt for VS 2022".
    exit /b 1
)

if not exist "%USERPROFILE%\JUCE\CMakeLists.txt" (
    echo ERROR: no JUCE checkout at %USERPROFILE%\JUCE
    echo.
    echo   git clone https://github.com/juce-framework/JUCE.git "%USERPROFILE%\JUCE"
    echo   cd /d "%USERPROFILE%\JUCE"
    echo   git checkout 857aab9c4eb3084af639a380a693dcec7d728b73
    echo.
    exit /b 1
)

if "%DO_CLEAN%"=="1" (
    echo == removing %BUILD_DIR%
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

echo == configure -^> %BUILD_DIR%
cmake -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
      -DFOURCOLOR_COPY_AFTER_BUILD=%DO_INSTALL% ^
      -DFOURCOLOR_WERROR=ON
if errorlevel 1 (
    echo.
    echo CONFIGURE FAILED. The usual causes are a missing C++ workload in the
    echo Visual Studio installer, or a JUCE checkout at the wrong commit.
    exit /b 1
)

for %%C in (%CONFIGS%) do (
    echo.
    echo == build %%C
    cmake --build "%BUILD_DIR%" --config %%C --parallel
    if errorlevel 1 (
        echo BUILD FAILED in %%C
        exit /b 1
    )

    echo == tests %%C
    "%BUILD_DIR%\FourColorTests_artefacts\%%C\FourColorTests.exe"
    if errorlevel 1 (
        echo.
        echo TESTS FAILED in %%C. Every check prints its measured value, so the
        echo failing line above says what the number was and what it needed to be.
        exit /b 1
    )
)

echo.
echo Artefacts:
if exist "%BUILD_DIR%\FourColor_artefacts\Release\VST3\FourColor.vst3" ^
    echo   %BUILD_DIR%\FourColor_artefacts\Release\VST3\FourColor.vst3
if exist "%BUILD_DIR%\FourColor_artefacts\Release\Standalone\FourColor.exe" ^
    echo   %BUILD_DIR%\FourColor_artefacts\Release\Standalone\FourColor.exe
echo.
echo To install for Cubase, copy the .vst3 FOLDER (not just its contents) into:
echo   C:\Program Files\Common Files\VST3\
echo That needs an administrator Command Prompt.
echo.
echo Then work through docs\CUBASE-CHECKLIST.md and fill in the Windows column.
endlocal
