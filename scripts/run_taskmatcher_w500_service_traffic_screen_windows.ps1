param(
    [int]$Timesteps = 250,
    [string]$OutputDir = "results/dormitory/taskmatcher_w500_service_traffic_screen_windows"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "build/lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }

$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$fullOutputDir = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $fullOutputDir | Out-Null
$instanceFile = Join-Path $repo "instances/warehouseSmall/warehouseSmall_500.json"

# Zero is the original k=50 matcher.  Positive values penalize delivery legs
# whose endpoints have high predicted flow, without adding another all-pairs
# shortest-path computation to the online scheduler.
$weights = @("0.0", "0.025", "0.05", "0.10", "0.20")
foreach ($weight in $weights) {
    $tag = $weight.Replace('.', 'p')
    $label = "w500_k50_servicew${tag}_t${Timesteps}"
    $output = Join-Path $fullOutputDir "$label.json"
    if (Test-Path -LiteralPath $output) {
        try {
            if ((Get-Content -LiteralPath $output -Raw | ConvertFrom-Json).numTaskFinished -gt 0) {
                Write-Host "Skipping completed $label"
                continue
            }
        } catch {}
    }

    $args = @(
        "--inputFile", $instanceFile, "--output", $output, "--outputScreen", "3",
        "--simulationTime", "$Timesteps", "--planTimeLimit", "1000", "--preprocessTimeLimit", "30000",
        "--scheduleModel", "7", "--useTraffic", "false", "--assignNew", "false", "--commitWindow", "1",
        "--logDetailLevel", "2", "--matcherDistWeight", "10", "--matcherTopK", "100",
        "--matcherMaxMatrix", "2000000", "--matcherUseTraffic", "true", "--matcherTrafficTopK", "50",
        "--matcherTrafficCongestionWeight", "1.0", "--matcherTrafficServiceWeight", $weight,
        "--matcherMaxAssign", "1.0", "--matcherReassign", "true", "--heapKeepBias", "0", "--heapProtectDist", "0"
    )
    Write-Host "Running $label"
    & $exe @args 2>&1 | Tee-Object -FilePath ([System.IO.Path]::ChangeExtension($output, ".log"))
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit code $LASTEXITCODE" }
}

$summary = Get-ChildItem -LiteralPath $fullOutputDir -Filter "w500_k50_servicew*_t${Timesteps}.json" |
    Where-Object { $_.Name -notlike '*.metrics_summary.json' } |
    Sort-Object Name |
    ForEach-Object {
        $result = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        $metrics = Get-Content -LiteralPath ($_.FullName -replace '\.json$', '.metrics_summary.json') -Raw | ConvertFrom-Json
        [pscustomobject]@{
            ServiceWeight = [regex]::Match($_.BaseName, 'servicew([0-9p]+)_').Groups[1].Value.Replace('p', '.')
            FinishedTasks = [int]$result.numTaskFinished
            CompletionP95 = $metrics.taskLatency.arrivalToCompletion.p95
            EmptyDistance = [int64]$metrics.movement.emptyDistance
            LoadedDetourMean = $metrics.movement.loadedDetourRatio.mean
            ActiveWait = [int64]$metrics.agentStateSteps.activeWait
            PlannerSecondsMean = $metrics.plannerSeconds.mean
            EntryTimeouts = [int]$result.numEntryTimeouts
            PlannerErrors = [int]$result.numPlannerErrors
            ScheduleErrors = [int]$result.numScheduleErrors
        }
    }
$summary | Sort-Object FinishedTasks -Descending | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $fullOutputDir "summary.csv")
$summary | Sort-Object FinishedTasks -Descending | Format-Table -AutoSize | Out-String | Set-Content -Encoding utf8 (Join-Path $fullOutputDir "summary.txt")
