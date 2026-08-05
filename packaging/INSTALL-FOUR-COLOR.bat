@echo off
setlocal enabledelayedexpansion
title FOUR COLOR 0.1.0 - Installer

echo.
echo  ============================================
echo    FOUR COLOR by Gussa Naaman  -  v0.1.0
echo    Multiband Colour ^& Saturation
echo  ============================================
echo.

REM --- administrator ----------------------------------------------------------
net session >nul 2>&1
if errorlevel 1 (
    echo  [X] This installer needs administrator rights.
    echo.
    echo      Close this window, RIGHT-CLICK INSTALL-FOUR-COLOR.bat
    echo      and choose "Run as administrator".
    echo.
    pause
    exit /b 1
)
echo  [OK] Running as administrator.

REM --- find the plug-in --------------------------------------------------------
REM Beside this file first (that is how the packaged folder is laid out), then
REM the usual build output paths, so this same installer works whether you were
REM sent a package or you just built the source yourself.
set "SRC="

for %%D in (
    "%~dp0FourColor.vst3"
    "%~dp0dist\FourColor-windows-x64\FourColor.vst3"
    "%~dp0build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
    "%~dp0..\build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
    "%~dp0..\..\build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
    "%~dp0build\FourColor_artefacts\Release\VST3\FourColor.vst3"
    "%~dp0..\build\FourColor_artefacts\Release\VST3\FourColor.vst3"
) do (
    if not defined SRC if exist "%%~D\" set "SRC=%%~D"
)

if not defined SRC (
    echo  [X] Could not find FourColor.vst3 anywhere.
    echo.
    echo      Looked beside this file:
    echo        %~dp0
    echo      and in the usual build output folders under it.
    echo.
    echo      Two likely reasons:
    echo.
    echo      1. You are running this from INSIDE the ZIP viewer. Windows
    echo         copies the .bat out on its own and leaves the plugin behind.
    echo         EXTRACT the whole ZIP to a real folder first
    echo         ^(right-click the ZIP -^> "Extract All"^), then run it again.
    echo.
    echo      2. You have the source but have not built it yet. Run:
    echo            scripts\build-windows.bat both --clean
    echo         then run this installer again.
    echo.
    pause
    exit /b 1
)
echo  [OK] Found the plugin to install:
echo       !SRC!

set "DEST=C:\Program Files\Common Files\VST3"
if not exist "%DEST%\" mkdir "%DEST%" 2>nul

REM --- warn about a running DAW ------------------------------------------------
REM A host that has FOUR COLOR loaded holds the DLL open, and Windows will not
REM let anyone replace it. This is the most common reason an update silently
REM does nothing, so it is worth stopping for.
set "DAW="
for %%P in (Cubase.exe Nuendo.exe Ableton.exe FL64.exe FL.exe "Studio One.exe" reaper.exe "Bitwig Studio.exe" WaveLab.exe) do (
    tasklist /fi "imagename eq %%~P" 2>nul | find /i "%%~P" >nul && set "DAW=%%~P"
)
if defined DAW (
    echo.
    echo  [!] !DAW! appears to be running.
    echo      Windows will not let the plugin be replaced while a host has it
    echo      loaded. Please close it now, then press any key to continue.
    echo.
    pause
)

REM --- remove any previous install --------------------------------------------
REM The old version may be a FOLDER bundle or a single .vst3 FILE, depending on
REM which build installed it. rmdir cannot delete a file and del cannot delete a
REM folder, so both are tried and the result is CHECKED rather than assumed.
echo.
echo  [1/4] Removing any previous version...

if exist "%DEST%\FourColor.vst3\" (
    echo        found a folder bundle - deleting
    rmdir /s /q "%DEST%\FourColor.vst3"
) else if exist "%DEST%\FourColor.vst3" (
    echo        found a single file - deleting
    del /f /q "%DEST%\FourColor.vst3"
) else (
    echo        nothing to remove
)

if exist "%DEST%\FourColor.vst3" (
    echo.
    echo  [X] The old FOUR COLOR could not be removed.
    echo      Something still has it open - usually a DAW, or the plugin
    echo      scanner that runs in the background after a DAW closes.
    echo.
    echo      Close every audio application, wait a few seconds and run
    echo      this installer again.
    echo.
    pause
    exit /b 1
)
echo        done.

REM --- install the VST3 ---------------------------------------------------------
echo  [2/4] Installing the VST3 ...
xcopy /e /i /y "!SRC!" "%DEST%\FourColor.vst3\" >nul
if errorlevel 1 (
    echo.
    echo  [X] Copy failed.
    echo      Source: !SRC!
    echo      Target: %DEST%\FourColor.vst3
    echo.
    pause
    exit /b 1
)

REM --- verify -------------------------------------------------------------------
REM Reporting success without checking is how an installer lies. The binary
REM itself has to be on disk before this says "Done".
echo  [3/4] Verifying...
if not exist "%DEST%\FourColor.vst3\Contents\x86_64-win\FourColor.vst3" (
    echo.
    echo  [X] The plugin binary is not where it should be after copying.
    echo      Expected: %DEST%\FourColor.vst3\Contents\x86_64-win\FourColor.vst3
    echo.
    echo      Send me a screenshot of this window - that path is the clue.
    echo.
    pause
    exit /b 1
)
echo        verified.

REM --- the standalone, if it shipped alongside ---------------------------------
echo  [4/4] Standalone app...
set "EXE="
for %%E in (
    "%~dp0FourColor.exe"
    "%~dp0dist\FourColor-windows-x64\FourColor.exe"
    "%~dp0build-win\FourColor_artefacts\Release\Standalone\FourColor.exe"
    "%~dp0..\build-win\FourColor_artefacts\Release\Standalone\FourColor.exe"
) do (
    if not defined EXE if exist "%%~E" set "EXE=%%~E"
)

if defined EXE (
    set "APPDIR=C:\Program Files\Naaman\FOUR COLOR"
    if not exist "!APPDIR!\" mkdir "!APPDIR!" 2>nul
    copy /y "!EXE!" "!APPDIR!\FourColor.exe" >nul
    if exist "!APPDIR!\FourColor.exe" (
        echo        installed to !APPDIR!
    ) else (
        echo        [!] could not copy the standalone - the VST3 is fine, ignore this
    )
) else (
    echo        not included in this package - skipping
)

echo.
echo  ============================================
echo    Done. FOUR COLOR 0.1.0 is installed.
echo.
echo    VST3:
echo    %DEST%\FourColor.vst3
echo.
echo    1. Start Cubase and rescan plugins
echo       ^(Studio ^> VST Plug-in Manager ^> Update^)
echo    2. Find "FourColor" under Naaman, in Distortion
echo    3. Put it on an AUDIO channel
echo.
echo    Reported latency should be 65 samples at
echo    EVERY Quality setting. If it is not, tell me.
echo  ============================================
echo.
pause
