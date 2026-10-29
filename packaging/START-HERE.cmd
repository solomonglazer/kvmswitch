@echo off
setlocal DisableDelayedExpansion
cd /d "%~dp0"
if not exist "kvmswitch.exe" goto missing
if /i "%~1"=="--check-package" goto checkpackage
if not "%~1"=="" exit /b 2
echo LG 38WR85QC-W - Windows to Mac test
echo.
echo 1. Check readiness - does not switch the monitor
echo 2. Run one PC-to-Mac switch test
echo 3. Exit
choice /c 123 /n /m "Choose 1, 2, or 3: "
if errorlevel 3 exit /b 0
if errorlevel 2 goto switch
call :preflight
goto finish

:switch
call :preflight
if errorlevel 1 goto finish
echo.
echo Confirm the LG shown above is your monitor.
echo Keep the Mac awake on USB-C and the monitor joystick accessible.
echo The picture and USB keyboard/mouse may move to the Mac.
choice /c YN /n /m "Send ONE experimental switch command now? [Y/N]: "
if errorlevel 2 goto finish
echo === One experimental switch command ===>>test-results.txt
kvmswitch.exe switch usb-c --confirm-experimental >>test-results.txt 2>&1
set "switch_result=%errorlevel%"
echo Switch exit code: %switch_result%>>test-results.txt
type test-results.txt
echo.
echo An accepted command does not confirm picture or USB transfer.
echo Inspect the monitor. Use the joystick to return to DisplayPort.
echo Do not repeat automatically if an error appears.
goto finish

:preflight
echo === Read-only readiness checks ===>test-results.txt
type BUILD-INFO.txt >>test-results.txt 2>&1
echo === Active monitors ===>>test-results.txt
kvmswitch.exe list >>test-results.txt 2>&1
if errorlevel 1 goto preflightfailed
echo === NVIDIA route and EDID ===>>test-results.txt
kvmswitch.exe doctor >>test-results.txt 2>&1
if errorlevel 1 goto preflightfailed
echo === Dry run ===>>test-results.txt
kvmswitch.exe switch usb-c --dry-run >>test-results.txt 2>&1
if errorlevel 1 goto preflightfailed
type test-results.txt
echo Readiness checks passed. No switch command has been sent.
exit /b 0

:preflightfailed
type test-results.txt
echo Readiness check failed. No switch command has been sent.
echo Share test-results.txt so the failure can be investigated.
exit /b 1

:checkpackage
rem CI/package check: help returns before any monitor discovery or NVAPI initialization.
kvmswitch.exe --help
exit /b %errorlevel%

:missing
echo Extract ALL files from the ZIP before running START-HERE.cmd.
pause
exit /b 2

:finish
echo.
echo Results are saved in test-results.txt beside this launcher.
echo See FIRST-TEST.txt for recording the picture and USB results.
pause
exit /b 0
