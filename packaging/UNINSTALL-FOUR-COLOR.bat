@echo off
setlocal enabledelayedexpansion
title FOUR COLOR - Uninstaller

echo.
echo  ============================================
echo    FOUR COLOR  -  Uninstall
echo  ============================================
echo.

net session >nul 2>&1
if errorlevel 1 (
    echo  [X] This needs administrator rights.
    echo      RIGHT-CLICK UNINSTALL-FOUR-COLOR.bat, then "Run as administrator".
    echo.
    pause
    exit /b 1
)

REM A stuck install is the reason this file exists, so it clears EVERY plausible
REM location and BOTH shapes - folder bundle and single file - and says what it
REM found in each. Silence here would defeat the purpose.
set "FOUND=0"

call :clear "C:\Program Files\Common Files\VST3\FourColor.vst3"
call :clear "C:\Program Files (x86)\Common Files\VST3\FourColor.vst3"
call :clear "%COMMONPROGRAMFILES%\VST3\FourColor.vst3"
call :clear "%LOCALAPPDATA%\Programs\Common\VST3\FourColor.vst3"
call :clear "%USERPROFILE%\AppData\Local\Programs\Common\VST3\FourColor.vst3"

if exist "C:\Program Files\Naaman\FOUR COLOR\FourColor.exe" (
    echo  - standalone: C:\Program Files\Naaman\FOUR COLOR
    rmdir /s /q "C:\Program Files\Naaman\FOUR COLOR"
    set "FOUND=1"
)

echo.
if "%FOUND%"=="0" (
    echo  Nothing was found to remove. FOUR COLOR is not installed
    echo  in any of the locations this checks.
) else (
    echo  Done. Rescan plugins in your DAW so it drops the entry.
)
echo.
pause
exit /b 0

:clear
if exist "%~1\" (
    echo  - folder bundle: %~1
    rmdir /s /q "%~1"
    if exist "%~1" (
        echo    [X] could not delete - close every audio application and retry
    ) else (
        set "FOUND=1"
    )
) else if exist "%~1" (
    echo  - single file: %~1
    del /f /q "%~1"
    if exist "%~1" (
        echo    [X] could not delete - close every audio application and retry
    ) else (
        set "FOUND=1"
    )
)
exit /b 0
