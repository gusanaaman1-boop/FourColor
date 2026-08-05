@echo off
setlocal enabledelayedexpansion
title FOUR COLOR 0.1.0 - Installer

REM ---------------------------------------------------------------------------
REM  No text echoed by this script may contain > < & | or ^.
REM
REM  The previous version routed every line through a "call :say" helper. %~1
REM  strips the quotes, so a > inside the text became a redirection operator:
REM  it mangled the output, created stray files, and broke the parsing of the
REM  if-blocks badly enough that execution carried on past a failure and
REM  printed "DONE" after "NOT INSTALLED". Plain echo, plain words.
REM ---------------------------------------------------------------------------

set "LOG=%~dp0FourColor-install-log.txt"
echo FOUR COLOR installer log> "%LOG%"
echo Started: %DATE% %TIME%>> "%LOG%"
echo Script folder: %~dp0>> "%LOG%"

echo.
echo  ============================================
echo    FOUR COLOR by Gussa Naaman  -  v0.1.0
echo    Multiband Colour and Saturation
echo  ============================================
echo.

REM --- administrator ----------------------------------------------------------
net session >nul 2>&1
if errorlevel 1 (
    echo  [X] This installer needs administrator rights.
    echo.
    echo      Close this window, RIGHT-CLICK INSTALL-FOUR-COLOR.bat
    echo      and choose "Run as administrator".
    echo NOT ADMIN>> "%LOG%"
    goto :fail
)
echo  [OK] Running as administrator.

set "DEST=C:\Program Files\Common Files\VST3"

REM --- find the plug-in --------------------------------------------------------
echo.
echo  [1/5] Looking for FourColor.vst3 ...
set "SRC="

call :try "%~dp0FourColor.vst3"
call :try "%~dp0dist\FourColor-windows-x64\FourColor.vst3"
call :try "%~dp0build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
call :try "%~dp0..\build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
call :try "%~dp0..\..\build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
call :try "%USERPROFILE%\Desktop\FourColor\build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"
call :try "C:\dev\FourColor\build-win\FourColor_artefacts\Release\VST3\FourColor.vst3"

if not defined SRC (
    echo        not in the usual places - searching nearby folders...
    call :search "%~dp0"
)
if not defined SRC call :search "%USERPROFILE%\Desktop"
if not defined SRC call :search "%USERPROFILE%\Downloads"
if not defined SRC call :search "C:\dev"

REM --- still nothing: ask for it ------------------------------------------------
if not defined SRC (
    echo.
    echo  [?] I could not find FourColor.vst3 by myself.
    echo.
    echo      Open the folder where you BUILT it, go into
    echo         build-win \ FourColor_artefacts \ Release \ VST3
    echo      and DRAG the FourColor.vst3 folder into this window,
    echo      then press Enter.
    echo.
    echo      (Or just press Enter to give up.)
    echo.
    set /p "DROPPED=Path: "
    if defined DROPPED (
        set "DROPPED=!DROPPED:"=!"
        if exist "!DROPPED!\Contents" set "SRC=!DROPPED!"
    )
)

if not defined SRC (
    echo.
    echo  [X] No FourColor.vst3 to install.
    echo      Every path tried is listed in:
    echo      %LOG%
    echo.
    echo      Most likely: the Windows build has not been run, or this .bat
    echo      is sitting somewhere unrelated to it. Build with:
    echo         scripts\build-windows.bat Release
    echo NOT FOUND>> "%LOG%"
    goto :fail
)

echo        found:
echo        !SRC!
echo FOUND: !SRC!>> "%LOG%"

if not exist "%DEST%\" mkdir "%DEST%" 2>nul

REM --- warn about a running DAW ------------------------------------------------
set "DAW="
for %%P in (Cubase.exe Nuendo.exe Ableton.exe FL64.exe reaper.exe) do (
    tasklist /fi "imagename eq %%P" 2>nul | find /i "%%P" >nul && set "DAW=%%P"
)
if defined DAW (
    echo.
    echo  [!] !DAW! is running. Windows will not replace a plugin a host
    echo      has loaded. Close it, then press any key.
    echo DAW RUNNING: !DAW!>> "%LOG%"
    pause
)

REM --- remove any previous install --------------------------------------------
echo  [2/5] Removing any previous version...
if exist "%DEST%\FourColor.vst3\" (
    echo        deleting old folder bundle
    rmdir /s /q "%DEST%\FourColor.vst3"
) else if exist "%DEST%\FourColor.vst3" (
    echo        deleting old single file
    del /f /q "%DEST%\FourColor.vst3"
) else (
    echo        nothing to remove
)

if exist "%DEST%\FourColor.vst3" (
    echo.
    echo  [X] The old version could not be removed. Something has it open -
    echo      a DAW, or the plugin scanner that keeps running after one closes.
    echo      Close every audio application and run this again.
    echo REMOVE FAILED>> "%LOG%"
    goto :fail
)

REM --- install ------------------------------------------------------------------
echo  [3/5] Installing...
xcopy /e /i /y "!SRC!" "%DEST%\FourColor.vst3\" >> "%LOG%" 2>&1
if errorlevel 1 (
    echo.
    echo  [X] Copy failed. xcopy's own message is in the log.
    echo      From: !SRC!
    echo      To:   %DEST%\FourColor.vst3
    echo COPY FAILED>> "%LOG%"
    goto :fail
)

REM --- verify -------------------------------------------------------------------
echo  [4/5] Verifying...
if not exist "%DEST%\FourColor.vst3\Contents\x86_64-win\FourColor.vst3" (
    echo.
    echo  [X] The binary is not where it should be after copying.
    echo      Wanted: %DEST%\FourColor.vst3\Contents\x86_64-win\FourColor.vst3
    echo VERIFY FAILED - what is actually there:>> "%LOG%"
    dir /s /b "%DEST%\FourColor.vst3" >> "%LOG%" 2>&1
    goto :fail
)
echo        verified.

REM --- standalone ---------------------------------------------------------------
echo  [5/5] Standalone app...
set "EXE="
if exist "%~dp0FourColor.exe" set "EXE=%~dp0FourColor.exe"
if not defined EXE for %%E in ("!SRC!\..\..\Standalone\FourColor.exe") do (
    if exist "%%~fE" set "EXE=%%~fE"
)
if defined EXE (
    if not exist "C:\Program Files\Naaman\FOUR COLOR\" mkdir "C:\Program Files\Naaman\FOUR COLOR" 2>nul
    copy /y "!EXE!" "C:\Program Files\Naaman\FOUR COLOR\FourColor.exe" >nul 2>&1
    echo        installed
) else (
    echo        not found - skipping, the VST3 is what matters
)

echo INSTALL OK>> "%LOG%"
echo.
echo  ============================================
echo    DONE. FOUR COLOR is installed.
echo.
echo    %DEST%\FourColor.vst3
echo.
echo    1. Start Cubase
echo    2. Studio menu, then VST Plug-in Manager, then Update
echo    3. If you do not see it, type "Four" in the plugin
echo       search box - it may be filed under Other.
echo  ============================================
echo.
pause
exit /b 0

REM =============================================================================
:try
if defined SRC exit /b 0
echo tried: %~1>> "%LOG%"
if exist "%~1\Contents" set "SRC=%~1"
exit /b 0

:search
if defined SRC exit /b 0
if not exist "%~1" exit /b 0
echo searching: %~1>> "%LOG%"
for /f "delims=" %%F in ('dir /s /b /ad "%~1\FourColor.vst3" 2^>nul') do (
    if not defined SRC if exist "%%F\Contents" set "SRC=%%F"
)
exit /b 0

:fail
echo.
echo  ------------------------------------------------
echo   NOT INSTALLED.
echo   Send me this file:
echo   %LOG%
echo  ------------------------------------------------
echo.
pause
exit /b 1
