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

REM --- the payload must be sitting next to this file ---------------------------
REM Running the .bat straight out of the ZIP viewer copies it to a temp folder
REM on its own, and then there is nothing to install. Catch that here rather
REM than failing later with an unhelpful error.
set "SRC=%~dp0FourColor.vst3"

if not exist "%SRC%\" (
    echo  [X] FourColor.vst3 was not found next to this installer.
    echo.
    echo      Looked in: %~dp0
    echo.
    echo      You are probably running this from inside the ZIP.
    echo      EXTRACT the whole ZIP to a real folder first
    echo      ^(right-click the ZIP -^> "Extract All"^), then run
    echo      INSTALL-FOUR-COLOR.bat from the extracted folder.
    echo.
    pause
    exit /b 1
)
echo  [OK] Found FourColor.vst3 to install.

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
xcopy /e /i /y "%SRC%" "%DEST%\FourColor.vst3\" >nul
if errorlevel 1 (
    echo.
    echo  [X] Copy failed.
    echo      Source: %SRC%
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
if exist "%~dp0FourColor.exe" (
    set "APPDIR=C:\Program Files\Naaman\FOUR COLOR"
    if not exist "!APPDIR!\" mkdir "!APPDIR!" 2>nul
    copy /y "%~dp0FourColor.exe" "!APPDIR!\FourColor.exe" >nul
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
