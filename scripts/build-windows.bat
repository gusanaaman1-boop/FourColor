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

REM JUCE is not bundled: it is a separate project with its own licence, and it
REM is large. If it is missing, offer to fetch it at the exact commit the Mac
REM builds against - a different JUCE is a different plug-in.
set JUCE_COMMIT=857aab9c4eb3084af639a380a693dcec7d728b73

if not exist "%USERPROFILE%\JUCE\CMakeLists.txt" (
    echo.
    echo JUCE was not found at %USERPROFILE%\JUCE
    echo.
    where git >nul 2>nul
    if errorlevel 1 (
        echo ERROR: git is not on PATH either, so it cannot be fetched for you.
        echo        Install Git, or copy a JUCE 9 checkout to %USERPROFILE%\JUCE
        echo        and check out commit %JUCE_COMMIT%
        exit /b 1
    )

    set /p FETCH="Clone JUCE now (about 500 MB)? [y/N] "
    if /I not "!FETCH!"=="y" (
        echo Aborted. See docs\WINDOWS-QUICKSTART.md.
        exit /b 1
    )

    echo == cloning JUCE
    git clone https://github.com/juce-framework/JUCE.git "%USERPROFILE%\JUCE"
    if errorlevel 1 exit /b 1

    pushd "%USERPROFILE%\JUCE"
    git checkout %JUCE_COMMIT%
    if errorlevel 1 (
        popd
        echo ERROR: could not check out %JUCE_COMMIT%
        exit /b 1
    )
    popd
    echo == JUCE ready at %JUCE_COMMIT%
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

    REM Performance checks measure the machine as much as the plug-in, and this
    REM script has no idea what else is running on it. They are reported and not
    REM enforced here; they are enforced on a quiet machine before release.
    set "FOURCOLOR_SKIP_PERF=1"
    echo == tests %%C
    "%BUILD_DIR%\FourColorTests_artefacts\%%C\FourColorTests.exe"
    if errorlevel 1 (
        echo.
        echo TESTS FAILED in %%C. Every check prints its measured value, so the
        echo failing line above says what the number was and what it needed to be.
        exit /b 1
    )
)

REM --- assemble a ready-to-run installer folder ---------------------------------
REM The point is that nobody has to copy anything by hand: after this, the
REM folder below contains the plug-in AND the installer that puts it where it
REM belongs, and it is also exactly what you would zip and send to somebody.
set "PKG=dist\FourColor-windows-x64"

if exist "%BUILD_DIR%\FourColor_artefacts\Release\VST3\FourColor.vst3" (
    echo.
    echo == packaging -^> %PKG%
    if exist "%PKG%" rmdir /s /q "%PKG%"
    mkdir "%PKG%" 2>nul

    xcopy /e /i /y "%BUILD_DIR%\FourColor_artefacts\Release\VST3\FourColor.vst3" ^
                   "%PKG%\FourColor.vst3\" >nul
    if exist "%BUILD_DIR%\FourColor_artefacts\Release\Standalone\FourColor.exe" ^
        copy /y "%BUILD_DIR%\FourColor_artefacts\Release\Standalone\FourColor.exe" "%PKG%\" >nul

    copy /y "packaging\INSTALL-FOUR-COLOR.bat"   "%PKG%\" >nul
    copy /y "packaging\UNINSTALL-FOUR-COLOR.bat" "%PKG%\" >nul
    copy /y "packaging\README-WINDOWS.txt"       "%PKG%\" >nul

    if not exist "%PKG%\FourColor.vst3\Contents\x86_64-win\FourColor.vst3" (
        echo   [X] the packaged VST3 is missing its binary - something is wrong
        exit /b 1
    )
    echo   packaged and verified.
)

echo.
echo  ============================================
echo    TO INSTALL:
echo.
echo    1. cd %PKG%
echo    2. RIGHT-CLICK INSTALL-FOUR-COLOR.bat
echo       -^> "Run as administrator"
echo.
echo    That folder is also what you would zip
echo    and send to somebody else.
echo  ============================================
echo.
echo Then work through docs\CUBASE-CHECKLIST.md and fill in the Windows column.
endlocal
