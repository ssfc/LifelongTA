param(
    [int]$Timesteps = 1000,
    [int]$Trials = 3,
    [string]$OutputDir = "results/dormitory/w600_loss_focus_flow_vs_taskmatcher"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "build/lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }

$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$fullOutputDir = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $fullOutputDir | Out-Null
$instanceFile = Join-Path $repo "instances/warehouseSmall/warehouseSmall_600.json"

function Invoke-Case([string]$Method, [int]$Trial) {
    $label = "w600_{0}_t{1}_trial{2}" -f $Method, $Timesteps, $Trial
    $output = Join-Path $fullOutputDir "$label.json"
    if (Test-Path -LiteralPath $output) {
        try {
            if ((Get-Content -LiteralPath $output -Raw | ConvertFrom-Json).numTaskFinished -gt 0) {
                Write-Host "Skipping completed $label"
                return
            }
        } catch {}
    }
    $args = @(
        "--inputFile", $instanceFile, "--output", $output, "--outputScreen", "3",
        "--simulationTime", "$Timesteps", "--planTimeLimit", "1000", "--preprocessTimeLimit", "30000",
        "--assignNew", "false", "--logDetailLevel", "2"
    )
    if ($Method -eq "flow-traffic") {
        $args += @("--scheduleModel", "1", "--useTraffic", "true")
    } elseif ($Method -eq "taskmatcher-k50") {
        $args += @(
            "--scheduleModel", "7", "--useTraffic", "false", "--commitWindow", "1",
            "--matcherDistWeight", "10", "--matcherTopK", "100", "--matcherMaxMatrix", "2000000",
            "--matcherUseTraffic", "true", "--matcherTrafficTopK", "50", "--matcherTrafficCongestionWeight", "1.0",
            "--matcherTrafficServiceWeight", "0.0", "--matcherMaxAssign", "1.0", "--matcherReassign", "true",
            "--heapKeepBias", "0", "--heapProtectDist", "0"
        )
    } else { throw "Unknown method: $Method" }
    Write-Host "Running $label"
    & $exe @args 2>&1 | Tee-Object -FilePath ([System.IO.Path]::ChangeExtension($output, ".log"))
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit code $LASTEXITCODE" }
}

for ($trial = 1; $trial -le $Trials; ++$trial) {
    Invoke-Case "flow-traffic" $trial
    Invoke-Case "taskmatcher-k50" $trial
}

$summary = Get-ChildItem -LiteralPath $fullOutputDir -Filter "w600_*_t${Timesteps}_trial*.json" |
    Where-Object { $_.Name -notlike '*.metrics_summary.json' } |
    Sort-Object Name |
    ForEach-Object {
        $result = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        $metrics = Get-Content -LiteralPath ($_.FullName -replace '\.json$', '.metrics_summary.json') -Raw | ConvertFrom-Json
        [pscustomobject]@{
            Method = if ($_.BaseName -match 'flow-traffic') { 'flow-traffic' } else { 'taskmatcher-k50' }
            Trial = [int]([regex]::Match($_.BaseName, 'trial(\d+)$').Groups[1].Value)
            FinishedTasks = [int]$result.numTaskFinished
            UnassignedBacklogAtEnd = [int]$metrics.unassignedBacklogAtEnd
            PickupDistanceMean = $metrics.matchingQuality.pickupDistanceAtAssignment.mean
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
$summary | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $fullOutputDir "summary.csv")
$summary | Format-Table -AutoSize | Out-String | Set-Content -Encoding utf8 (Join-Path $fullOutputDir "summary.txt")
