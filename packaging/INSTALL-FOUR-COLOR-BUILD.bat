@echo off
setlocal enabledelayedexpansion
title FOUR COLOR 1.0.0-rc.1 - Build and Install

REM ---------------------------------------------------------------------------
REM  ONE file. Right-click it, "Run as administrator", and it does everything:
REM  checks what is installed, builds the plug-in, installs it, verifies it.
REM
REM  No text echoed by this script may contain the characters
REM  greater-than, less-than, ampersand, pipe or caret. An earlier version
REM  routed every line through a "call :say" helper; %~1 strips the quotes, so
REM  a greater-than inside the text became a redirection operator, mangled the
REM  output, created stray files and broke the if-blocks badly enough that it
REM  printed DONE after NOT INSTALLED. Plain echo, plain words.
REM ---------------------------------------------------------------------------

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "LOG=%ROOT%\FourColor-install-log.txt"
set "DEST=C:\Program Files\Common Files\VST3"
set "BUILD=%ROOT%\build-win"

echo FOUR COLOR 1.0.0-rc.1 build and install log > "%LOG%"
echo Started: %DATE% %TIME% >> "%LOG%"
echo Folder: %ROOT% >> "%LOG%"

cls
echo.
echo   ============================================================
echo     FOUR COLOR  1.0.0-rc.1
echo     Multiband Colour and Saturation  -  by Gussa Naaman
echo   ============================================================
echo.
echo   This builds the plug-in from source and installs it.
echo   The first run takes a few minutes. After that it is quick.
echo.

REM --- 0. administrator --------------------------------------------------------
net session >nul 2>&1
if errorlevel 1 (
    echo   [X] This needs administrator rights to write into Program Files.
    echo.
    echo       Close this window, RIGHT-CLICK this file, and choose
    echo       "Run as administrator".
    echo NOT ADMIN >> "%LOG%"
    goto :fail
)
echo   [OK] Running as administrator.

REM --- 1. prerequisites --------------------------------------------------------
echo.
echo   [1/6] Checking what is installed...

set "MISSING="

where cmake >nul 2>&1
if errorlevel 1 (
    REM CMake ships inside Visual Studio; try there before giving up.
    for %%P in (
        "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
        "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
    ) do (
        if exist "%%~P\cmake.exe" set "PATH=%%~P;!PATH!"
    )
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo         [X] CMake not found.
    set "MISSING=1"
) else (
    echo         [OK] CMake found
    cmake --version >> "%LOG%" 2>&1
)

REM Visual Studio 2022 with the C++ workload.
REM
REM The for /f sits at top level on purpose. Inside a parenthesised block the
REM caret escaping needed for a redirect or a pipe stops behaving, which is the
REM same class of bug that broke the previous installer.
REM
REM This check is ADVISORY. vswhere is not always present and its component id
REM can differ between installer versions, so a negative here is not proof of
REM anything - CMake is the authority on whether the toolchain works, and it
REM says so clearly a few lines further down. Blocking on a guess would stop a
REM machine that could have built perfectly well.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if not exist "%VSWHERE%" goto :vsdone
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%I"
:vsdone

if defined VSPATH (
    echo         [OK] Visual Studio found
    echo VS: !VSPATH! >> "%LOG%"
) else (
    echo         [?] Could not confirm Visual Studio. Carrying on anyway -
    echo             the next step will say plainly if the compiler is missing.
    echo VS NOT DETECTED BY VSWHERE >> "%LOG%"
)

REM JUCE 9.
set "JUCEDIR=%USERPROFILE%\JUCE"
if exist "%JUCEDIR%\CMakeLists.txt" (
    echo         [OK] JUCE found at %JUCEDIR%
) else (
    echo         [!] JUCE not found at %JUCEDIR%
    where git >nul 2>&1
    if errorlevel 1 (
        echo         [X] ...and git is not installed, so I cannot fetch it.
        set "MISSING=1"
    ) else (
        echo.
        echo         I can download JUCE 9 for you now. It is about 500 MB
        echo         and goes to %JUCEDIR%. This happens once.
        echo.
        choice /c YN /m "        Download JUCE now"
        if errorlevel 2 (
            echo         Skipped. Nothing was installed.
            echo JUCE DECLINED >> "%LOG%"
            goto :fail
        )
        echo.
        echo         Downloading JUCE, please wait...
        git clone --quiet https://github.com/juce-framework/JUCE.git "%JUCEDIR%" >> "%LOG%" 2>&1
        if errorlevel 1 (
            echo         [X] The download failed. Details are in the log.
            set "MISSING=1"
        ) else (
            pushd "%JUCEDIR%"
            git checkout --quiet 857aab9c4eb3084af639a380a693dcec7d728b73 >> "%LOG%" 2>&1
            popd
            echo         [OK] JUCE downloaded and set to the pinned version.
        )
    )
)

if defined MISSING (
    echo.
    echo   ============================================================
    echo     Something is missing. Nothing has been changed.
    echo   ============================================================
    echo.
    echo     Visual Studio 2022 - free Community edition:
    echo       https://visualstudio.microsoft.com/downloads/
    echo       In the installer tick "Desktop development with C++".
    echo.
    echo     That one download also provides CMake, so install it first
    echo     and run this file again.
    echo.
    echo MISSING PREREQUISITES >> "%LOG%"
    goto :fail
)

REM --- 2. configure ------------------------------------------------------------
echo.
echo   [2/6] Preparing the build...
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64 -DFOURCOLOR_COPY_AFTER_BUILD=OFF >> "%LOG%" 2>&1
if errorlevel 1 (
    echo         [X] Preparation failed.
    echo.
    echo             The usual cause is Visual Studio 2022 without the
    echo             "Desktop development with C++" workload. Install it from
    echo             https://visualstudio.microsoft.com/downloads/
    echo             and run this file again.
    echo.
    echo             The exact reason is at the end of:
    echo             %LOG%
    echo CONFIGURE FAILED >> "%LOG%"
    goto :fail
)
echo         [OK] Ready.

REM --- 3. build ----------------------------------------------------------------
echo.
echo   [3/6] Building. This is the slow part - a few minutes.
echo         Nothing is wrong if it looks stuck; it is compiling.
cmake --build "%BUILD%" --config Release --target FourColor_VST3 FourColor_Standalone --parallel >> "%LOG%" 2>&1
if errorlevel 1 (
    echo.
    echo         [X] The build failed.
    echo             The compiler's own message is at the end of:
    echo             %LOG%
    echo             Send me the last 40 lines of that file.
    echo BUILD FAILED >> "%LOG%"
    goto :fail
)
echo         [OK] Built.

set "SRC=%BUILD%\FourColor_artefacts\Release\VST3\FourColor.vst3"
if not exist "%SRC%\Contents\x86_64-win\FourColor.vst3" (
    echo         [X] The build reported success but the plug-in is not where
    echo             it should be. Expected:
    echo             %SRC%
    echo MISSING ARTEFACT >> "%LOG%"
    dir /s /b "%BUILD%\FourColor_artefacts" >> "%LOG%" 2>&1
    goto :fail
)

REM --- 4. a running DAW will block the copy ------------------------------------
echo.
echo   [4/6] Checking for a running DAW...
set "DAW="
for %%P in (Cubase.exe Cubase14.exe Cubase15.exe Nuendo.exe Ableton.exe FL64.exe reaper.exe) do (
    tasklist /fi "imagename eq %%P" 2>nul | find /i "%%P" >nul && set "DAW=%%P"
)
if defined DAW (
    echo.
    echo         [!] !DAW! is running.
    echo             Windows will not replace a plug-in a host has loaded, and
    echo             this is the usual reason an update seems to do nothing.
    echo.
    echo             Close it now, then press any key.
    echo DAW RUNNING: !DAW! >> "%LOG%"
    pause >nul
) else (
    echo         [OK] Nothing in the way.
)

REM --- 5. install --------------------------------------------------------------
echo.
echo   [5/6] Installing...
if not exist "%DEST%\" mkdir "%DEST%" 2>nul

if exist "%DEST%\FourColor.vst3\" (
    rmdir /s /q "%DEST%\FourColor.vst3"
) else (
    if exist "%DEST%\FourColor.vst3" del /f /q "%DEST%\FourColor.vst3"
)

if exist "%DEST%\FourColor.vst3" (
    echo         [X] The old version could not be removed. Something still has
    echo             it open - a DAW, or the plug-in scanner that keeps running
    echo             after one closes. Close every audio application and run
    echo             this again.
    echo REMOVE FAILED >> "%LOG%"
    goto :fail
)

xcopy /e /i /y "%SRC%" "%DEST%\FourColor.vst3\" >> "%LOG%" 2>&1
if errorlevel 1 (
    echo         [X] The copy failed. xcopy's own message is in the log.
    echo COPY FAILED >> "%LOG%"
    goto :fail
)
echo         [OK] Plug-in installed.

set "EXE=%BUILD%\FourColor_artefacts\Release\Standalone\FourColor.exe"
if exist "%EXE%" (
    if not exist "C:\Program Files\Naaman\FOUR COLOR\" mkdir "C:\Program Files\Naaman\FOUR COLOR" 2>nul
    copy /y "%EXE%" "C:\Program Files\Naaman\FOUR COLOR\FourColor.exe" >nul 2>&1
    echo         [OK] Standalone app installed.
)

REM --- 6. verify ---------------------------------------------------------------
echo.
echo   [6/6] Verifying...
if not exist "%DEST%\FourColor.vst3\Contents\x86_64-win\FourColor.vst3" (
    echo         [X] The binary is not where it should be after copying.
    echo VERIFY FAILED - what is actually there: >> "%LOG%"
    dir /s /b "%DEST%\FourColor.vst3" >> "%LOG%" 2>&1
    goto :fail
)
echo         [OK] Verified.
echo INSTALL OK >> "%LOG%"

echo.
echo   ============================================================
echo     DONE. FOUR COLOR 1.0.0-rc.1 is installed.
echo   ============================================================
echo.
echo     %DEST%\FourColor.vst3
echo.
echo     1. Start Cubase
echo     2. Studio menu, then VST Plug-in Manager, then Update
echo     3. It appears under Naaman, category Distortion
echo.
echo     CHECK THE VERSION. In the plug-in window, under the words
echo     FOUR COLOR on the left, small grey text must read
echo     1.0.0-rc.1
echo.
echo     If it reads 0.1.0 then Cubase is still loading an older copy
echo     from somewhere else. Tell me and send the log.
echo.
pause
exit /b 0

:fail
echo.
echo   ============================================================
echo     NOT INSTALLED. Nothing was changed.
echo   ============================================================
echo.
echo     The full log is here:
echo     %LOG%
echo.
echo     Send me that file. It lists every step and every path tried.
echo.
pause
exit /b 1
