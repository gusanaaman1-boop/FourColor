@echo off
setlocal enabledelayedexpansion
title FOUR COLOR 0.1.0 - Installer

REM Everything this prints also goes to a log file beside this script, so a
REM failure can be sent to me as a file instead of retyped. There is no Windows
REM machine on my side; this log is the only debugging channel there is.
set "LOG=%~dp0FourColor-install-log.txt"
echo FOUR COLOR installer log > "%LOG%" 2>nul
echo Started: %DATE% %TIME% >> "%LOG%" 2>nul
echo Script folder: %~dp0 >> "%LOG%" 2>nul
echo. >> "%LOG%" 2>nul

call :say ""
call :say " ============================================"
call :say "   FOUR COLOR by Gussa Naaman  -  v0.1.0"
call :say "   Multiband Colour and Saturation"
call :say " ============================================"
call :say ""

REM --- administrator ----------------------------------------------------------
net session >nul 2>&1
if errorlevel 1 (
    call :say " [X] This installer needs administrator rights."
    call :say ""
    call :say "     Close this window, RIGHT-CLICK INSTALL-FOUR-COLOR.bat"
    call :say "     and choose 'Run as administrator'."
    call :say ""
    goto :fail
)
call :say " [OK] Running as administrator."

REM --- find the plug-in --------------------------------------------------------
REM Beside this file first, then the usual build output paths, then a recursive
REM search from here. Every path tried is logged, so if it finds nothing the log
REM says exactly where it looked.
set "SRC="
call :say ""
call :say " [1/5] Looking for FourColor.vst3 ..."

for %%D in (
    "%~dp0FourColor.vst3"
    "%~dp0dist\FourColor-windows-x64\FourColor.vst3"
    "%~dp0build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
    "%~dp0..\build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
    "%~dp0..\..\build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
    "%~dp0build\FourColor_artefacts\Release\VST3\FourColor.vst3"
    "%~dp0..\build\FourColor_artefacts\Release\VST3\FourColor.vst3"
) do (
    echo   tried: %%~D >> "%LOG%" 2>nul
    if not defined SRC if exist "%%~D\Contents" set "SRC=%%~D"
)

if not defined SRC (
    call :say "       not in the usual places - searching this folder tree..."
    for /f "delims=" %%F in ('dir /s /b /ad "%~dp0FourColor.vst3" 2^>nul') do (
        if not defined SRC if exist "%%F\Contents" set "SRC=%%F"
    )
)

if not defined SRC (
    call :say ""
    call :say " [X] Could not find FourColor.vst3 anywhere under:"
    call :say "     %~dp0"
    call :say ""
    call :say "     The log lists every path that was tried:"
    call :say "     %LOG%"
    call :say ""
    call :say "     Three things it usually is:"
    call :say ""
    call :say "     1. The ZIP was extracted somewhere else. This .bat has to"
    call :say "        sit IN your FourColor folder - the one containing"
    call :say "        build-win - or next to a FourColor.vst3."
    call :say ""
    call :say "     2. You are running it from inside the ZIP viewer. Windows"
    call :say "        copies the .bat out alone. Extract the ZIP properly."
    call :say ""
    call :say "     3. The Windows build has not been run yet:"
    call :say "           scripts\build-windows.bat both --clean"
    call :say ""
    goto :fail
)
call :say "       found: !SRC!"

set "DEST=C:\Program Files\Common Files\VST3"
if not exist "%DEST%\" mkdir "%DEST%" 2>nul

REM --- warn about a running DAW ------------------------------------------------
set "DAW="
for %%P in (Cubase.exe Nuendo.exe Ableton.exe FL64.exe FL.exe reaper.exe WaveLab.exe) do (
    tasklist /fi "imagename eq %%P" 2>nul | find /i "%%P" >nul && set "DAW=%%P"
)
if defined DAW (
    call :say ""
    call :say " [!] !DAW! appears to be running."
    call :say "     Windows will not replace the plugin while a host has it"
    call :say "     loaded. Close it now, then press any key."
    pause
)

REM --- remove any previous install --------------------------------------------
call :say " [2/5] Removing any previous version..."

if exist "%DEST%\FourColor.vst3\" (
    call :say "       found a folder bundle - deleting"
    rmdir /s /q "%DEST%\FourColor.vst3"
) else if exist "%DEST%\FourColor.vst3" (
    call :say "       found a single file - deleting"
    del /f /q "%DEST%\FourColor.vst3"
) else (
    call :say "       nothing to remove"
)

if exist "%DEST%\FourColor.vst3" (
    call :say ""
    call :say " [X] The old FOUR COLOR could not be removed."
    call :say "     Something still has it open - a DAW, or the plugin scanner"
    call :say "     that keeps running for a while after a DAW closes."
    call :say ""
    call :say "     Close every audio application, wait, and run this again."
    goto :fail
)

REM --- install ------------------------------------------------------------------
call :say " [3/5] Installing the VST3 ..."
xcopy /e /i /y "!SRC!" "%DEST%\FourColor.vst3\" >> "%LOG%" 2>&1
if errorlevel 1 (
    call :say ""
    call :say " [X] Copy failed. xcopy's own output is in the log."
    call :say "     Source: !SRC!"
    call :say "     Target: %DEST%\FourColor.vst3"
    goto :fail
)

REM --- verify -------------------------------------------------------------------
call :say " [4/5] Verifying..."
if not exist "%DEST%\FourColor.vst3\Contents\x86_64-win\FourColor.vst3" (
    call :say ""
    call :say " [X] The plugin binary is not where it should be after copying."
    call :say "     Expected:"
    call :say "     %DEST%\FourColor.vst3\Contents\x86_64-win\FourColor.vst3"
    call :say ""
    call :say "     What IS there:"
    dir /s /b "%DEST%\FourColor.vst3" >> "%LOG%" 2>&1
    dir /b "%DEST%\FourColor.vst3\Contents" 2>nul
    goto :fail
)
call :say "       verified."

REM --- standalone ---------------------------------------------------------------
call :say " [5/5] Standalone app..."
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
    copy /y "!EXE!" "!APPDIR!\FourColor.exe" >nul 2>&1
    if exist "!APPDIR!\FourColor.exe" (
        call :say "       installed to !APPDIR!"
    ) else (
        call :say "       [!] standalone did not copy - the VST3 is fine, ignore this"
    )
) else (
    call :say "       not found - skipping (the VST3 is what matters)"
)

call :say ""
call :say " ============================================"
call :say "   DONE. FOUR COLOR is installed."
call :say ""
call :say "   %DEST%\FourColor.vst3"
call :say ""
call :say "   1. Start Cubase"
call :say "   2. Studio ^> VST Plug-in Manager ^> Update"
call :say "   3. FourColor, under Naaman, in Distortion"
call :say " ============================================"
call :say ""
echo  Log written to: %LOG%
echo.
pause
exit /b 0

:fail
call :say ""
call :say " ------------------------------------------------"
call :say "  NOT INSTALLED."
call :say "  Send me this file and I will know what happened:"
call :say "  %LOG%"
call :say " ------------------------------------------------"
echo.
pause
exit /b 1

:say
echo %~1
echo %~1 >> "%LOG%" 2>nul
exit /b 0
