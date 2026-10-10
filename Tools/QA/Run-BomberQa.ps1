[CmdletBinding()]
param(
    [switch]$SkipSheetSync,
    [string]$ExistingReport
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $scriptDir '..\..')).Path
$projectFile = Join-Path $projectRoot 'Bomber.uproject'
$configPath = Join-Path $scriptDir 'qa-sheet.local.json'
$expectedSpreadsheetId = '1UGXPM7sho_VNmtClY1Nrd-pBGjv7YkrBoXlWgwz534g'
$expectedSpreadsheetTitle = 'Bomber QA 테스트 케이스'
$expectedSheetName = '통합 테스트 케이스'
$testFilter = 'Bomber.Integration.Bomb'

$testDefinitions = @(
    [pscustomobject]@{
        TestId = 'INT-BOMB-001'
        AutomationPath = 'Bomber.Integration.Bomb.EmptyCellPlacement'
        PassText = '빈 셀에 폭탄 액터 1개 생성 확인'
    },
    [pscustomobject]@{
        TestId = 'INT-BOMB-002'
        AutomationPath = 'Bomber.Integration.Bomb.RejectOccupiedCell'
        PassText = '벽·상자 점유 셀의 폭탄 수 0 유지 확인'
    }
)

function Write-Step([string]$Message) {
    Write-Host "[QA] $Message" -ForegroundColor Cyan
}

function Get-UnrealEditorCmd {
    if ($env:BOMBER_UE_EDITOR_CMD) {
        if (Test-Path -LiteralPath $env:BOMBER_UE_EDITOR_CMD) {
            return (Resolve-Path -LiteralPath $env:BOMBER_UE_EDITOR_CMD).Path
        }
        throw "BOMBER_UE_EDITOR_CMD does not exist: $($env:BOMBER_UE_EDITOR_CMD)"
    }

    $uproject = Get-Content -Raw -LiteralPath $projectFile | ConvertFrom-Json
    $association = [string]$uproject.EngineAssociation
    if (-not $association) {
        throw 'Bomber.uproject does not define EngineAssociation.'
    }

    $registryKeys = @(
        "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association",
        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$association"
    )

    foreach ($registryKey in $registryKeys) {
        if (Test-Path $registryKey) {
            $installDirectory = (Get-ItemProperty -Path $registryKey).InstalledDirectory
            $candidate = Join-Path $installDirectory 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
            if (Test-Path -LiteralPath $candidate) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }

    throw "Unreal Editor Cmd for EngineAssociation '$association' was not found. Set BOMBER_UE_EDITOR_CMD."
}

function Get-GitCommit {
    try {
        $hash = (& git -C $projectRoot rev-parse --short HEAD 2>$null)
        if ($LASTEXITCODE -eq 0 -and $hash) { return [string]$hash.Trim() }
    } catch {
    }
    return 'unknown'
}

function Get-EntrySummary($TestResult) {
    if (-not $TestResult -or -not $TestResult.entries) { return '' }
    $messages = @(
        $TestResult.entries |
            ForEach-Object {
                if ($_.event -and $_.message) { "[$($_.event)] $($_.message)" }
                elseif ($_.message) { [string]$_.message }
                else { [string]$_ }
            } |
            Where-Object { $_ }
    )
    return (($messages | Select-Object -First 3) -join ' / ')
}

function New-Result($Definition, $TestResult, [string]$Reason, [string]$Commit, [string]$RunId, [string]$RelativeReportPath) {
    if (-not $TestResult) {
        $state = 'Blocked'
        $duration = 0.0
        $warnings = 0
        $errors = 0
        $actual = "자동화 테스트 중단: $Reason"
    } else {
        $duration = [double]$TestResult.duration
        $warnings = [int]$TestResult.warnings
        $errors = [int]$TestResult.errors
        $entrySummary = Get-EntrySummary $TestResult

        if ($TestResult.state -eq 'Success' -and $errors -eq 0) {
            $state = 'Pass'
            $actual = "자동화 테스트 성공: $($Definition.PassText) ({0:N2}초, 오류 {1}, 경고 {2})" -f $duration, $errors, $warnings
        } elseif ($TestResult.state -in @('Fail', 'Failed', 'Failure') -or $errors -gt 0) {
            $state = 'Fail'
            $detail = if ($entrySummary) { $entrySummary } else { "상태=$($TestResult.state)" }
            $actual = "자동화 테스트 실패: $detail ({0:N2}초, 오류 {1}, 경고 {2})" -f $duration, $errors, $warnings
        } else {
            $state = 'Blocked'
            $detail = if ($entrySummary) { $entrySummary } else { "완료되지 않은 상태=$($TestResult.state)" }
            $actual = "자동화 테스트 중단: $detail"
        }
    }

    $comment = $RunId
    return [pscustomobject]@{
        testId = $Definition.TestId
        automationPath = $Definition.AutomationPath
        state = $state
        actualResult = $actual
        comment = $comment
        duration = [Math]::Round($duration, 4)
        warnings = $warnings
        errors = $errors
    }
}

if (-not (Test-Path -LiteralPath $projectFile)) {
    throw "Project file was not found: $projectFile"
}

$runStartedUtc = [DateTime]::UtcNow
$runId = Get-Date -Format 'yyyy-MM-dd HH:mm:ss K'
$folderStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$commit = Get-GitCommit
$reportDirectory = $null
$unrealExitCode = 0

if ($ExistingReport) {
    $reportFile = (Resolve-Path -LiteralPath $ExistingReport).Path
    $reportDirectory = Split-Path -Parent $reportFile
    Write-Step "Using existing report: $reportFile"
} else {
    $runningEditors = @(Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd' -ErrorAction SilentlyContinue)
    if ($runningEditors.Count -gt 0) {
        Write-Host '[BLOCKED] Unreal Editor is running. Close it before starting the unattended BAT test.' -ForegroundColor Yellow
        exit 10
    }

    $editorCmd = Get-UnrealEditorCmd
    $reportDirectory = Join-Path $projectRoot "Saved\Automation\BAT-$folderStamp"
    $logFile = Join-Path $projectRoot "Saved\Logs\BomberQA-$folderStamp.log"
    $consoleLogFile = Join-Path $projectRoot "Saved\Logs\BomberQA-$folderStamp.console.log"
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null

    $arguments = @(
        $projectFile,
        '-unattended',
        '-nop4',
        '-nosplash',
        '-nullrhi',
        '-nosound',
        '-stdout',
        '-FullStdOutLogOutput',
        "-log=$logFile",
        "-ReportExportPath=$reportDirectory",
        "-ExecCmds=Automation RunTests $testFilter",
        '-TestExit=Automation Test Queue Empty'
    )

    Write-Step "Running unattended tests: $testFilter"
    & $editorCmd @arguments *> $consoleLogFile
    $unrealExitCode = $LASTEXITCODE
    Write-Step "Unreal log: $logFile"
    $reportFile = Join-Path $reportDirectory 'index.json'
}

$relativeReportPath = $reportDirectory
if ($reportDirectory.StartsWith($projectRoot, [StringComparison]::OrdinalIgnoreCase)) {
    $relativeReportPath = $reportDirectory.Substring($projectRoot.Length).TrimStart('\')
}

$report = $null
$reportReason = ''
if (-not (Test-Path -LiteralPath $reportFile)) {
    $reportReason = "결과 보고서 없음 (Unreal 종료 코드 $unrealExitCode)"
} else {
    $reportItem = Get-Item -LiteralPath $reportFile
    if (-not $ExistingReport -and $reportItem.LastWriteTimeUtc -lt $runStartedUtc.AddSeconds(-5)) {
        $reportReason = '현재 실행보다 오래된 결과 보고서가 감지됨'
    } else {
        try {
            $report = Get-Content -Raw -LiteralPath $reportFile -Encoding UTF8 | ConvertFrom-Json
        } catch {
            $reportReason = "결과 보고서 JSON 해석 실패: $($_.Exception.Message)"
        }
    }
}

$results = @()
foreach ($definition in $testDefinitions) {
    $testResult = $null
    if ($report -and $report.tests) {
        $testResult = @($report.tests | Where-Object { $_.fullTestPath -eq $definition.AutomationPath }) | Select-Object -First 1
    }

    $reason = $reportReason
    if (-not $reason -and -not $testResult) {
        $reason = "보고서에 테스트 누락: $($definition.AutomationPath)"
    }
    $results += New-Result $definition $testResult $reason $commit $runId $relativeReportPath
}

Write-Host ''
Write-Host '=== Bomber QA Result ===' -ForegroundColor White
foreach ($result in $results) {
    $color = if ($result.state -eq 'Pass') { 'Green' } elseif ($result.state -eq 'Fail') { 'Red' } else { 'Yellow' }
    Write-Host ("{0}: {1} - {2}" -f $result.testId, $result.state, $result.actualResult) -ForegroundColor $color
}

$payloadDirectory = Join-Path $projectRoot 'Saved\Automation\SheetSync'
New-Item -ItemType Directory -Path $payloadDirectory -Force | Out-Null
$payloadFile = Join-Path $payloadDirectory "$folderStamp.json"

$payloadBase = [ordered]@{
    schemaVersion = 1
    spreadsheetId = $expectedSpreadsheetId
    spreadsheetTitle = $expectedSpreadsheetTitle
    sheetName = $expectedSheetName
    runId = $runId
    gitCommit = $commit
    results = $results
}

$payloadBase | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $payloadFile -Encoding UTF8
Write-Step "Validated payload saved: $payloadFile"

$syncFailed = $false
if (-not $SkipSheetSync) {
    if (-not (Test-Path -LiteralPath $configPath)) {
        Write-Host "[SHEET ERROR] Missing local config: $configPath" -ForegroundColor Red
        Write-Host 'Copy qa-sheet.local.example.json to qa-sheet.local.json and configure the Apps Script web app URL/token.' -ForegroundColor Yellow
        $syncFailed = $true
    } else {
        $config = Get-Content -Raw -LiteralPath $configPath -Encoding UTF8 | ConvertFrom-Json
        if ($config.spreadsheetId -ne $expectedSpreadsheetId -or $config.spreadsheetTitle -ne $expectedSpreadsheetTitle) {
            throw 'Local config does not point to the exact Bomber QA 테스트 케이스 spreadsheet.'
        }
        if (-not $config.webAppUrl -or $config.webAppUrl -match 'PASTE_|CHANGE_ME') {
            throw 'Google Apps Script webAppUrl is not configured.'
        }
        if (-not $config.token -or $config.token -match 'CHANGE_ME') {
            throw 'Google Apps Script token is not configured.'
        }

        $requestPayload = [ordered]@{}
        foreach ($entry in $payloadBase.GetEnumerator()) { $requestPayload[$entry.Key] = $entry.Value }
        $requestPayload.token = [string]$config.token
        $requestJson = $requestPayload | ConvertTo-Json -Depth 10

        try {
            Write-Step 'Sending validated results to Google Sheet...'
            $response = Invoke-RestMethod -Uri ([string]$config.webAppUrl) -Method Post -ContentType 'application/json; charset=utf-8' -Body ([System.Text.Encoding]::UTF8.GetBytes($requestJson)) -TimeoutSec 45
            if (-not $response.ok) {
                throw "Apps Script rejected the update: $($response.error)"
            }
            Write-Host ("[SHEET] Updated and verified {0} row(s)." -f $response.updated) -ForegroundColor Green
        } catch {
            Write-Host "[SHEET ERROR] $($_.Exception.Message)" -ForegroundColor Red
            Write-Host "The validated payload remains available for retry: $payloadFile" -ForegroundColor Yellow
            $syncFailed = $true
        }
    }
}

if ($syncFailed) { exit 3 }
if (@($results | Where-Object { $_.state -eq 'Fail' }).Count -gt 0) { exit 1 }
if (@($results | Where-Object { $_.state -eq 'Blocked' }).Count -gt 0) { exit 2 }
exit 0
