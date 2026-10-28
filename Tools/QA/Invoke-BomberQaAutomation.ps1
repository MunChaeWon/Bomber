[CmdletBinding()]
param(
    [ValidateSet('Unit', 'Integration', 'System', 'Acceptance', 'All')]
    [string]$Suite = 'All',
    [switch]$SkipSheetSync,
    [switch]$SkipJiraSync,
    [switch]$SkipSlackNotification
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $scriptDir '..\..')).Path
$suiteRunner = Join-Path $scriptDir 'Run-BomberQa.ps1'
$automationRoot = Join-Path $projectRoot 'Saved\Automation'
$logRoot = Join-Path $projectRoot 'Saved\Logs'
$suiteNames = if ($Suite -eq 'All') { @('Unit', 'Integration', 'System', 'Acceptance') } else { @($Suite) }
$runKey = if ($env:GITHUB_RUN_ID) {
    $attempt = if ($env:GITHUB_RUN_ATTEMPT) { $env:GITHUB_RUN_ATTEMPT } else { '1' }
    "$($env:GITHUB_RUN_ID)-$attempt"
} else {
    'Local-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
}
$artifactRoot = if ($env:BOMBER_QA_ARTIFACT_ROOT) {
    [System.IO.Path]::GetFullPath($env:BOMBER_QA_ARTIFACT_ROOT)
} else {
    Join-Path $automationRoot "GitHub\$runKey"
}

if (-not (Test-Path -LiteralPath $suiteRunner)) {
    throw "Suite runner was not found: $suiteRunner"
}

$mutex = $null
$hasMutex = $false
$results = @()
$overallExitCode = 0
$automationStarted = Get-Date

try {
    try {
        $mutex = [System.Threading.Mutex]::new($false, 'Global\BomberQaAutomation')
    } catch {
        $mutex = [System.Threading.Mutex]::new($false, 'BomberQaAutomation')
    }
    try {
        $hasMutex = $mutex.WaitOne(0, $false)
    } catch [System.Threading.AbandonedMutexException] {
        $hasMutex = $true
    }
    if (-not $hasMutex) {
        Write-Host '[BLOCKED] Another Bomber QA automation run is already active.' -ForegroundColor Yellow
        exit 11
    }

    New-Item -ItemType Directory -Path $artifactRoot -Force | Out-Null

    foreach ($suiteName in $suiteNames) {
        $suiteStarted = Get-Date
        Write-Host "[QA] Starting $suiteName suite..." -ForegroundColor Cyan

        $arguments = @(
            '-NoLogo',
            '-NoProfile',
            '-ExecutionPolicy', 'Bypass',
            '-File', $suiteRunner,
            '-Suite', $suiteName
        )
        if ($SkipSheetSync) { $arguments += '-SkipSheetSync' }
        if ($SkipJiraSync) { $arguments += '-SkipJiraSync' }
        if ($SkipSlackNotification) { $arguments += '-SkipSlackNotification' }

        & powershell.exe @arguments
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0 -and $overallExitCode -eq 0) { $overallExitCode = $exitCode }

        $suiteArtifactRoot = Join-Path $artifactRoot $suiteName
        New-Item -ItemType Directory -Path $suiteArtifactRoot -Force | Out-Null

        $report = Get-ChildItem -LiteralPath $automationRoot -Directory -Filter "BAT-$suiteName-*" -ErrorAction SilentlyContinue |
            Where-Object { $_.LastWriteTime -ge $suiteStarted.AddSeconds(-5) } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($report) {
            Copy-Item -LiteralPath $report.FullName -Destination (Join-Path $suiteArtifactRoot $report.Name) -Recurse -Force
        }

        if (Test-Path -LiteralPath $logRoot) {
            Get-ChildItem -LiteralPath $logRoot -File -Filter "BomberQA-$suiteName-*" -ErrorAction SilentlyContinue |
                Where-Object { $_.LastWriteTime -ge $suiteStarted.AddSeconds(-5) } |
                Copy-Item -Destination $suiteArtifactRoot -Force
        }

        $results += [pscustomobject]@{
            suite = $suiteName
            exitCode = $exitCode
            startedAt = $suiteStarted.ToString('o')
            finishedAt = (Get-Date).ToString('o')
            reportCopied = [bool]$report
        }
    }

    $summary = [ordered]@{
        runKey = $runKey
        requestedSuite = $Suite
        startedAt = $automationStarted.ToString('o')
        finishedAt = (Get-Date).ToString('o')
        overallExitCode = $overallExitCode
        suites = $results
    }
    $summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $artifactRoot 'summary.json') -Encoding UTF8

    if ($env:GITHUB_STEP_SUMMARY) {
        $markdown = @(
            '# Bomber QA result',
            '',
            '| Suite | Exit code | Report |',
            '|---|---:|---|'
        )
        foreach ($result in $results) {
            $reportStatus = if ($result.reportCopied) { 'Saved' } else { 'Missing' }
            $markdown += "| $($result.suite) | $($result.exitCode) | $reportStatus |"
        }
        $markdown += ''
        $markdown += "Overall exit code: **$overallExitCode**"
        Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Value ($markdown -join "`n") -Encoding UTF8
    }
} finally {
    if ($hasMutex -and $mutex) { $mutex.ReleaseMutex() }
    if ($mutex) { $mutex.Dispose() }
}

exit $overallExitCode
