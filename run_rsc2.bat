@echo off
rem Launches the recompiled RalliSport Challenge 2 from this folder so that
rem game_files\default.xbe is found. Log output goes to run.log.
rem Extra arguments are passed to the exe, e.g. run_rsc2.bat --headless
rem
rem STATE: bring-up (not booting yet). See NOTES.md.

cd /d "%~dp0"

if not exist "build\Release\rsc2_recomp.exe" (
    echo build\Release\rsc2_recomp.exe not found. Build the project first.
    pause
    exit /b 1
)
if not exist "game_files\default.xbe" (
    echo game_files\default.xbe not found. Extract your disc into game_files\ first, see README.
    pause
    exit /b 1
)

rem Same baseline as mm3recomp's run_mm3.bat (see its comments for why each is needed).
set RECOMP_WATCHDOG_SECS=900
set RECOMP_STUB_LOG=1
set RECOMP_AC97_READY=1
set RECOMP_D3D11=1
set RECOMP_PB_EXEC=1
set RECOMP_HEAP_RECLAIM=1
set RECOMP_DISPATCH_GATE=1
set RECOMP_KEYBOARD=1
set RECOMP_D3D11_RES=2

echo Starting RalliSport Challenge 2 (recompiled)...
"build\Release\rsc2_recomp.exe" %* 2> run.log
echo.
echo Exited with code %ERRORLEVEL%. Log written to run.log
pause
