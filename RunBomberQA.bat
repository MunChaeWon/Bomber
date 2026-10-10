@echo off
setlocal
chcp 65001 >nul

set "PROJECT_ROOT=%~dp0"
set "RUNNER=%PROJECT_ROOT%Tools\QA\Run-BomberQa.ps1"

if not exist "%RUNNER%" (
    echo [ERROR] QA runner was not found: %RUNNER%
    set "EXIT_CODE=90"
    goto :finish
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%RUNNER%"
set "EXIT_CODE=%ERRORLEVEL%"

:finish
echo.
if "%EXIT_CODE%"=="0" (
    echo [DONE] Tests and Google Sheet synchronization completed.
) else (
    echo [FAILED] QA workflow exit code: %EXIT_CODE%
)

if /I not "%BOMBER_QA_NO_PAUSE%"=="1" pause
exit /b %EXIT_CODE%
