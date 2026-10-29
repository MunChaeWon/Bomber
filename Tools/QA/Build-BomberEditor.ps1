[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $scriptDir '..\..')).Path
$projectFile = Join-Path $projectRoot 'Bomber.uproject'

function Get-UnrealRoot {
    if ($env:BOMBER_UE_ROOT) {
        $configuredBuildScript = Join-Path $env:BOMBER_UE_ROOT 'Engine\Build\BatchFiles\Build.bat'
        if (Test-Path -LiteralPath $configuredBuildScript) {
            return [System.IO.Path]::GetFullPath($env:BOMBER_UE_ROOT)
        }
        throw "BOMBER_UE_ROOT does not contain Unreal Build.bat: $($env:BOMBER_UE_ROOT)"
    }

    $uproject = Get-Content -Raw -LiteralPath $projectFile | ConvertFrom-Json
    $association = [string]$uproject.EngineAssociation
    if (-not $association) { throw 'Bomber.uproject does not define EngineAssociation.' }

    $registryKeys = @(
        "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association",
        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$association"
    )
    foreach ($registryKey in $registryKeys) {
        if (Test-Path -LiteralPath $registryKey) {
            $installDirectory = [string](Get-ItemProperty -LiteralPath $registryKey).InstalledDirectory
            $buildScript = Join-Path $installDirectory 'Engine\Build\BatchFiles\Build.bat'
            if (Test-Path -LiteralPath $buildScript) {
                return [System.IO.Path]::GetFullPath($installDirectory)
            }
        }
    }

    throw "Unreal Engine '$association' was not found. Set BOMBER_UE_ROOT."
}

if (-not (Test-Path -LiteralPath $projectFile)) { throw "Project file was not found: $projectFile" }
if (Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd' -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor before building BomberEditor.'
}

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $projectRoot ('Saved\Automation\Build-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$unrealRoot = Get-UnrealRoot
$buildScript = Join-Path $unrealRoot 'Engine\Build\BatchFiles\Build.bat'
$buildLog = Join-Path $OutputDirectory 'BomberEditor-build.log'
$startedAt = Get-Date

Write-Host '[BUILD] Building BomberEditor Win64 Development...' -ForegroundColor Cyan
Push-Location $projectRoot
try {
    & $buildScript 'BomberEditor' 'Win64' 'Development' "-Project=$projectFile" '-WaitMutex' '-NoHotReloadFromIDE' '-NoUBA' 2>&1 |
        Tee-Object -FilePath $buildLog
    $buildExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

$duration = [Math]::Round(((Get-Date) - $startedAt).TotalSeconds, 2)
$buildSummary = [ordered]@{
    target = 'BomberEditor'
    platform = 'Win64'
    configuration = 'Development'
    exitCode = $buildExitCode
    durationSeconds = $duration
    log = $buildLog
}
$buildSummary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'build-summary.json') -Encoding UTF8

if ($env:GITHUB_STEP_SUMMARY) {
    $status = if ($buildExitCode -eq 0) { 'Success' } else { 'Failed' }
    Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Encoding UTF8 -Value @"
## BomberEditor build

- Status: **$status**
- Exit code: **$buildExitCode**
- Duration: **$duration seconds**
"@
}

if ($buildExitCode -ne 0) {
    Write-Host "[BUILD FAILED] See $buildLog" -ForegroundColor Red
    exit $buildExitCode
}

Write-Host "[BUILD DONE] Log: $buildLog" -ForegroundColor Green
exit 0
