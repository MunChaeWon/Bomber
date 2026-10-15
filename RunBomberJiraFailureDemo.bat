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

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%RUNNER%" -Suite System -JiraFailureDemo %*
set "EXIT_CODE=%ERRORLEVEL%"

:finish
echo.
if "%EXIT_CODE%"=="1" (
    echo [DONE] Intentional failure was detected and routed to Jira and Slack.
    set "EXIT_CODE=0"
) else if "%EXIT_CODE%"=="0" (
    echo [FAILED] The intentional-failure test unexpectedly passed.
    set "EXIT_CODE=91"
) else (
    echo [FAILED] Jira failure demo workflow exit code: %EXIT_CODE%
)

if /I not "%BOMBER_QA_NO_PAUSE%"=="1" pause
exit /b %EXIT_CODE%
