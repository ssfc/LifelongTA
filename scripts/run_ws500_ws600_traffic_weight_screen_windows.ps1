param(
    [int]$Timesteps = 250,
    [string]$OutputDir = 'results/dormitory/ws500_ws600_traffic_weight_screen'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build/lifelong.exe'
$fullOutputDir = Join-Path $repo $OutputDir
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
New-Item -ItemType Directory -Path $fullOutputDir -Force | Out-Null

foreach ($warehouse in @(500, 600)) {
    $inputFile = Join-Path $repo "instances/warehouseSmall/warehouseSmall_$warehouse.json"
    foreach ($weight in @('flow', '1.0', '2.0', '4.0')) {
        $method = if ($weight -eq 'flow') { 'flow-traffic' } else { "taskmatcher-traffic-w$weight" }
        $stem = "w${warehouse}_${method}_t${Timesteps}"
        $output = Join-Path $fullOutputDir "$stem.json"
        $log = Join-Path $fullOutputDir "$stem.log"
        if (Test-Path -LiteralPath $output) {
            try {
                $existing = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
                if ($existing.numTaskFinished -gt 0 -and $existing.numEntryTimeouts -eq 0 -and
                    $existing.numPlannerErrors -eq 0 -and $existing.numScheduleErrors -eq 0) {
                    Write-Output "Skipping complete $stem"
                    continue
                }
            } catch {}
            throw "Existing invalid output must be inspected before rerun: $output"
        }
        $arguments = @(
            '--inputFile', $inputFile, '--output', $output, '--outputScreen', '3',
            '--simulationTime', "$Timesteps", '--planTimeLimit', '1000',
            '--preprocessTimeLimit', '30000', '--assignNew', 'false', '--logDetailLevel', '2'
        )
        if ($weight -eq 'flow') {
            $arguments += @('--scheduleModel', '1', '--useTraffic', 'true')
        } else {
            $arguments += @(
                '--scheduleModel', '7', '--useTraffic', 'false', '--commitWindow', '1',
                '--matcherDistWeight', '10', '--matcherTopK', '100', '--matcherMaxMatrix', '2000000',
                '--matcherUseTraffic', 'true', '--matcherTrafficTopK', '100',
                '--matcherTrafficCongestionWeight', $weight, '--matcherTrafficServiceWeight', '0.0',
                '--matcherMaxAssign', '1.0', '--matcherReassign', 'true',
                '--heapKeepBias', '0', '--heapProtectDist', '0'
            )
        }
        Write-Output "$(Get-Date -Format o) Starting $stem"
        & $exe @arguments *> $log
        if ($LASTEXITCODE -ne 0) { throw "$stem exited with code $LASTEXITCODE; see $log" }
        $result = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
        if ($result.numTaskFinished -le 0 -or $result.numEntryTimeouts -ne 0 -or
            $result.numPlannerErrors -ne 0 -or $result.numScheduleErrors -ne 0) {
            throw "Invalid result for $stem; see $output"
        }
        Write-Output "$(Get-Date -Format o) Completed ${stem}: $($result.numTaskFinished) tasks"
    }
}

$rows = foreach ($warehouse in @(500, 600)) {
    foreach ($weight in @('flow', '1.0', '2.0', '4.0')) {
        $method = if ($weight -eq 'flow') { 'flow-traffic' } else { "taskmatcher-traffic-w$weight" }
        $stem = "w${warehouse}_${method}_t${Timesteps}"
        $result = Get-Content -LiteralPath (Join-Path $fullOutputDir "$stem.json") -Raw | ConvertFrom-Json
        $metrics = Get-Content -LiteralPath (Join-Path $fullOutputDir "$stem.metrics_summary.json") -Raw | ConvertFrom-Json
        [pscustomobject]@{
            Warehouse = $warehouse
            Method = $method
            FinishedTasks = [int]$result.numTaskFinished
            PickupDistanceMean = $metrics.matchingQuality.pickupDistanceAtAssignment.mean
            CompletionP95 = $metrics.taskLatency.arrivalToCompletion.p95
            EmptyWait = [int64]($metrics.agentStateSteps.empty - $metrics.movement.emptyDistance)
            LoadedWait = [int64]($metrics.agentStateSteps.loaded - $metrics.movement.loadedDistance)
            EmptyDistance = [int64]$metrics.movement.emptyDistance
            LoadedDetourMean = $metrics.movement.loadedDetourRatio.mean
            PlannerSecondsMean = $metrics.plannerSeconds.mean
            EntryTimeouts = [int]$result.numEntryTimeouts
            PlannerErrors = [int]$result.numPlannerErrors
            ScheduleErrors = [int]$result.numScheduleErrors
        }
    }
}
$rows | Export-Csv -LiteralPath (Join-Path $fullOutputDir 'summary.csv') -NoTypeInformation -Encoding UTF8
