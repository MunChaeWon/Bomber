[CmdletBinding()]
param(
    [ValidateSet('Integration', 'System')]
    [string]$Suite = 'Integration',
    [switch]$JiraFailureDemo,
    [switch]$SkipSheetSync,
    [switch]$SkipJiraSync,
    [switch]$SkipSlackNotification,
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
$integrationsConfigPath = Join-Path $scriptDir 'qa-integrations.local.json'
$expectedSpreadsheetId = '1UGXPM7sho_VNmtClY1Nrd-pBGjv7YkrBoXlWgwz534g'
$expectedSpreadsheetTitle = 'Bomber QA 테스트 케이스'
$suiteDefinitions = @{
    Integration = [pscustomobject]@{
        SheetName = '통합 테스트 케이스'
        TestFilter = 'Bomber.Integration.Bomb'
        Tests = @(
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
    }
    System = [pscustomobject]@{
        SheetName = '시스템 테스트 케이스'
        TestFilter = 'Bomber.System.'
        Tests = @(
            [pscustomobject]@{
                TestId = 'SYS-GAME-001'
                AutomationPath = 'Bomber.System.Game.StartupAndMapReady'
                PassText = '게임 시작, 생성 맵 및 플레이어 준비 상태 확인'
            },
            [pscustomobject]@{
                TestId = 'SYS-BOMB-001'
                AutomationPath = 'Bomber.System.Bomb.FullLifecycle'
                PassText = '폭탄 생성부터 폭발·제거까지 전체 생명주기 확인'
            },
            [pscustomobject]@{
                TestId = 'SYS-WORLD-001'
                AutomationPath = 'Bomber.System.World.DestroyBoxByExplosion'
                PassText = '폭발 범위의 인접 상자 제거 확인'
            },
            [pscustomobject]@{
                TestId = 'SYS-LOAD-001'
                AutomationPath = 'Bomber.System.Load.MassBombSpawnAndCleanup'
                PassText = '다중 폭탄 생성·정리 및 잔여 폭탄 0 확인'
            },
            [pscustomobject]@{
                TestId = 'SYS-RECOVERY-001'
                AutomationPath = 'Bomber.System.Load.PostLoadRecovery'
                PassText = '부하 정리 후 일반 폭탄 설치 기능 복구 확인'
            }
        )
    }
}

$suiteDefinition = $suiteDefinitions[$Suite]
$expectedSheetName = $suiteDefinition.SheetName
$testFilter = $suiteDefinition.TestFilter
$testDefinitions = @($suiteDefinition.Tests)
if ($JiraFailureDemo) {
    if ($Suite -ne 'System') { throw 'JiraFailureDemo is supported only with Suite System.' }
    $testFilter = 'Bomber.SystemDemo.Jira.IntentionalFailure'
    $testDefinitions = @(
        [pscustomobject]@{
            TestId = 'SYS-JIRA-FAIL-001'
            AutomationPath = 'Bomber.SystemDemo.Jira.IntentionalFailure'
            PassText = '의도된 실패가 감지되고 Jira 결함 등록 파이프라인이 실행됨'
        }
    )
}

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

function Get-GitBranch {
    try {
        $branch = (& git -C $projectRoot branch --show-current 2>$null)
        if ($LASTEXITCODE -eq 0 -and $branch) { return [string]$branch.Trim() }
    } catch {
    }
    return 'unknown'
}

function Get-EntrySummary($TestResult) {
    if (-not $TestResult -or -not $TestResult.entries) { return '' }
    $messages = @(
        $TestResult.entries |
            ForEach-Object {
                if ($_.event -and $_.event.message) { "[$($_.event.type)] $($_.event.message)" }
                elseif ($_.PSObject.Properties['message'] -and $_.message) { [string]$_.message }
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
        expectedResult = $Definition.PassText
        state = $state
        actualResult = $actual
        comment = $comment
        duration = [Math]::Round($duration, 4)
        warnings = $warnings
        errors = $errors
    }
}

function Get-JiraHeaders($JiraConfig) {
    $credentialText = '{0}:{1}' -f $JiraConfig.email, $JiraConfig.apiToken
    $credentialBytes = [System.Text.Encoding]::UTF8.GetBytes($credentialText)
    return @{
        Authorization = 'Basic ' + [Convert]::ToBase64String($credentialBytes)
        Accept = 'application/json'
    }
}

function New-AdfDocument([string[]]$Lines) {
    $paragraphs = @()
    foreach ($line in $Lines) {
        $paragraphs += [ordered]@{
            type = 'paragraph'
            content = @([ordered]@{ type = 'text'; text = [string]$line })
        }
    }
    return [ordered]@{ version = 1; type = 'doc'; content = $paragraphs }
}

function Invoke-JiraJson($JiraConfig, [string]$Method, [string]$Path, $Body = $null) {
    $uri = $JiraConfig.siteUrl.TrimEnd('/') + $Path
    $parameters = @{
        Uri = $uri
        Method = $Method
        Headers = Get-JiraHeaders $JiraConfig
        TimeoutSec = 45
    }
    if ($null -ne $Body) {
        $json = $Body | ConvertTo-Json -Depth 20
        $parameters.ContentType = 'application/json; charset=utf-8'
        $parameters.Body = [System.Text.Encoding]::UTF8.GetBytes($json)
    }
    return Invoke-RestMethod @parameters
}

function Get-EnvironmentSummary($AutomationReport) {
    if (-not $AutomationReport -or -not $AutomationReport.devices) { return '환경 정보 없음' }
    $device = @($AutomationReport.devices) | Select-Object -First 1
    return '{0} / {1} / CPU: {2} / GPU: {3} / RAM: {4}GB' -f `
        $device.platform, $device.oSVersion.Trim(), $device.cPUModel.Trim(), $device.gPU, $device.rAMInGB
}

function Sync-JiraFailures($JiraConfig, $Failures, [string]$RunTimestamp, [string]$GitBranch, [string]$GitCommit, [string]$ReportPath, [string]$Environment) {
    $links = @()
    $actions = @()
    foreach ($failure in $Failures) {
        $label = ('tc-' + $failure.testId.ToLowerInvariant()) -replace '[^a-z0-9-]', '-'
        $jql = 'project = "{0}" AND labels = "{1}" AND resolution IS EMPTY ORDER BY created DESC' -f $JiraConfig.projectKey, $label
        $encodedJql = [Uri]::EscapeDataString($jql)
        $searchPath = "/rest/api/3/search/jql?jql=$encodedJql&maxResults=1&fields=key,summary,status"
        $search = Invoke-JiraJson $JiraConfig 'Get' $searchPath
        $existing = @($search.issues) | Select-Object -First 1

        $detailLines = @(
            "TC ID: $($failure.testId)",
            "자동화 경로: $($failure.automationPath)",
            "기대 결과: $($failure.expectedResult)",
            "실제 결과: $($failure.actualResult)",
            "실행 시각: $RunTimestamp",
            "브랜치/커밋: $GitBranch / $GitCommit",
            "환경: $Environment",
            "보고서: $ReportPath"
        )

        if ($existing) {
            $commentBody = [ordered]@{ body = New-AdfDocument (@('자동화 테스트에서 동일 TC가 다시 실패했습니다.') + $detailLines) }
            Invoke-JiraJson $JiraConfig 'Post' "/rest/api/3/issue/$($existing.key)/comment" $commentBody | Out-Null
            $issueKey = [string]$existing.key
            $actions += "$issueKey 재발 댓글 추가"
        } else {
            $testName = ($failure.automationPath -split '\.')[-1]
            $issueBody = [ordered]@{
                fields = [ordered]@{
                    project = @{ key = [string]$JiraConfig.projectKey }
                    issuetype = @{ name = [string]$JiraConfig.issueType }
                    summary = "[자동화 실패][$($failure.testId)] $testName"
                    description = New-AdfDocument (@('Bomber QA 자동화 테스트에서 실패가 감지되었습니다.') + $detailLines)
                    labels = @('qa-automation', "$($Suite.ToLowerInvariant())-test", 'week6', $label)
                }
            }
            $created = Invoke-JiraJson $JiraConfig 'Post' '/rest/api/3/issue' $issueBody
            $issueKey = [string]$created.key
            $actions += "$issueKey 신규 생성"
        }

        $links += ($JiraConfig.siteUrl.TrimEnd('/') + '/browse/' + $issueKey)
    }
    return [pscustomobject]@{ Links = $links; Actions = $actions }
}

function Send-SlackSummary($SlackConfig, $ResultItems, [string]$RunTimestamp, [string]$GitBranch, [string]$GitCommit, [double]$TotalDuration, [string]$SheetStatus, [string[]]$JiraLinks, [string]$JiraStatus) {
    $total = @($ResultItems).Count
    $passed = @($ResultItems | Where-Object { $_.state -eq 'Pass' }).Count
    $failed = @($ResultItems | Where-Object { $_.state -eq 'Fail' }).Count
    $blocked = @($ResultItems | Where-Object { $_.state -eq 'Blocked' }).Count
    $symbol = if ($failed -gt 0 -or $blocked -gt 0) { ':x:' } else { ':white_check_mark:' }
    $failedIds = @($ResultItems | Where-Object { $_.state -eq 'Fail' } | ForEach-Object { $_.testId })
    $lines = @(
        "$symbol *Bomber $Suite QA 완료*",
        "전체 $total / 성공 $passed / 실패 $failed / 중단 $blocked",
        ('실행 시간: {0:N2}초' -f $TotalDuration),
        "브랜치/커밋: $GitBranch / $GitCommit",
        "실행 시각: $RunTimestamp",
        "Google Sheet: $SheetStatus",
        "Jira: $JiraStatus"
    )
    if ($failedIds.Count -gt 0) { $lines += '실패 TC: ' + ($failedIds -join ', ') }
    if ($JiraLinks.Count -gt 0) { $lines += 'Jira 링크: ' + ($JiraLinks -join ', ') }
    $body = @{ text = ($lines -join "`n") } | ConvertTo-Json -Depth 5
    Invoke-RestMethod -Uri ([string]$SlackConfig.webhookUrl) -Method Post -ContentType 'application/json; charset=utf-8' -Body ([System.Text.Encoding]::UTF8.GetBytes($body)) -TimeoutSec 45 | Out-Null
}

if (-not (Test-Path -LiteralPath $projectFile)) {
    throw "Project file was not found: $projectFile"
}

$runStartedUtc = [DateTime]::UtcNow
$runId = Get-Date -Format 'yyyy-MM-dd HH:mm:ss K'
$folderStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$commit = Get-GitCommit
$branch = Get-GitBranch
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
    $runLabel = if ($JiraFailureDemo) { "$Suite-JiraDemo" } else { $Suite }
    $reportDirectory = Join-Path $projectRoot "Saved\Automation\BAT-$runLabel-$folderStamp"
    $logFile = Join-Path $projectRoot "Saved\Logs\BomberQA-$runLabel-$folderStamp.log"
    $consoleLogFile = Join-Path $projectRoot "Saved\Logs\BomberQA-$runLabel-$folderStamp.console.log"
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

    Write-Step "Running unattended $Suite tests: $testFilter"
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
$payloadFile = Join-Path $payloadDirectory "$Suite-$folderStamp.json"

$payloadBase = [ordered]@{
    schemaVersion = 1
    spreadsheetId = $expectedSpreadsheetId
    spreadsheetTitle = $expectedSpreadsheetTitle
    sheetName = $expectedSheetName
    suite = $Suite
    runId = $runId
    gitBranch = $branch
    gitCommit = $commit
    results = $results
}

$payloadBase | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $payloadFile -Encoding UTF8
Write-Step "Validated payload saved: $payloadFile"

$syncFailed = $false
$sheetStatus = '건너뜀'
if (-not $SkipSheetSync) {
    if (-not (Test-Path -LiteralPath $configPath)) {
        Write-Host "[SHEET ERROR] Missing local config: $configPath" -ForegroundColor Red
        Write-Host 'Copy qa-sheet.local.example.json to qa-sheet.local.json and configure the Apps Script web app URL/token.' -ForegroundColor Yellow
        $syncFailed = $true
        $sheetStatus = '설정 없음'
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
            $sheetStatus = "반영 성공 ($($response.updated)건)"
        } catch {
            Write-Host "[SHEET ERROR] $($_.Exception.Message)" -ForegroundColor Red
            Write-Host "The validated payload remains available for retry: $payloadFile" -ForegroundColor Yellow
            $syncFailed = $true
            $sheetStatus = '반영 실패'
        }
    }
}

$integrationConfig = $null
$integrationConfigFailed = $false
if (-not $SkipJiraSync -or -not $SkipSlackNotification) {
    if (-not (Test-Path -LiteralPath $integrationsConfigPath)) {
        Write-Host "[INTEGRATION ERROR] Missing local config: $integrationsConfigPath" -ForegroundColor Red
        $integrationConfigFailed = $true
    } else {
        try {
            $integrationConfig = Get-Content -Raw -LiteralPath $integrationsConfigPath -Encoding UTF8 | ConvertFrom-Json
        } catch {
            Write-Host "[INTEGRATION ERROR] Invalid local config: $($_.Exception.Message)" -ForegroundColor Red
            $integrationConfigFailed = $true
        }
    }
}

$failures = @($results | Where-Object { $_.state -eq 'Fail' })
$jiraLinks = @()
$jiraStatus = if ($SkipJiraSync) { '건너뜀' } elseif ($failures.Count -eq 0) { '생성된 이슈 없음' } else { '처리 대기' }
$jiraFailed = $false
if (-not $SkipJiraSync -and $failures.Count -gt 0) {
    if (-not $integrationConfig -or -not $integrationConfig.jira) {
        $jiraStatus = '설정 없음'
        $jiraFailed = $true
    } else {
        try {
            Write-Step "Checking Jira duplicates and processing $($failures.Count) failure(s)..."
            $environment = Get-EnvironmentSummary $report
            $jiraResult = Sync-JiraFailures $integrationConfig.jira $failures $runId $branch $commit $relativeReportPath $environment
            $jiraLinks = @($jiraResult.Links)
            $jiraStatus = @($jiraResult.Actions) -join ', '
            Write-Host ("[JIRA] {0}" -f $jiraStatus) -ForegroundColor Green
        } catch {
            $jiraStatus = "처리 실패: $($_.Exception.Message)"
            Write-Host "[JIRA ERROR] $($_.Exception.Message)" -ForegroundColor Red
            $jiraFailed = $true
        }
    }
}

$slackFailed = $false
if (-not $SkipSlackNotification) {
    if (-not $integrationConfig -or -not $integrationConfig.slack -or -not $integrationConfig.slack.webhookUrl) {
        Write-Host '[SLACK ERROR] Slack webhook is not configured.' -ForegroundColor Red
        $slackFailed = $true
    } else {
        try {
            $totalDuration = if ($report -and $report.PSObject.Properties['totalDuration']) { [double]$report.totalDuration } else { [double](($results | Measure-Object -Property duration -Sum).Sum) }
            Write-Step 'Sending dynamic QA summary to Slack...'
            Send-SlackSummary $integrationConfig.slack $results $runId $branch $commit $totalDuration $sheetStatus $jiraLinks $jiraStatus
            Write-Host '[SLACK] Summary notification sent.' -ForegroundColor Green
        } catch {
            Write-Host "[SLACK ERROR] $($_.Exception.Message)" -ForegroundColor Red
            $slackFailed = $true
        }
    }
}

if ($syncFailed) { exit 3 }
if ($integrationConfigFailed -or $jiraFailed -or $slackFailed) { exit 4 }
if (@($results | Where-Object { $_.state -eq 'Fail' }).Count -gt 0) { exit 1 }
if (@($results | Where-Object { $_.state -eq 'Blocked' }).Count -gt 0) { exit 2 }
exit 0
