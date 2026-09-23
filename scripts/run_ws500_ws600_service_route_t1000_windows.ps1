param(
    [string]$OutputDir = 'results/dormitory/ws500_ws600_service_route_t1000'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build-route/lifelong.exe'
$fullOutputDir = Join-Path $repo $OutputDir
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
New-Item -ItemType Directory -Path $fullOutputDir -Force | Out-Null

$rows = foreach ($warehouse in @(500, 600)) {
    $stem = "w${warehouse}_taskmatcher-route-w015_t1000"
    $output = Join-Path $fullOutputDir "$stem.json"
    $log = Join-Path $fullOutputDir "$stem.log"
    if (-not (Test-Path -LiteralPath $output)) {
        $inputFile = Join-Path $repo "instances/warehouseSmall/warehouseSmall_$warehouse.json"
        $arguments = @(
            '--inputFile', $inputFile, '--output', $output, '--outputScreen', '3',
            '--simulationTime', '1000', '--planTimeLimit', '1000',
            '--preprocessTimeLimit', '30000', '--assignNew', 'false', '--logDetailLevel', '2',
            '--scheduleModel', '7', '--useTraffic', 'false', '--commitWindow', '1',
            '--matcherDistWeight', '10', '--matcherTopK', '100', '--matcherMaxMatrix', '2000000',
            '--matcherUseTraffic', 'true', '--matcherTrafficTopK', '100',
            '--matcherTrafficCongestionWeight', '1.0', '--matcherTrafficServiceWeight', '0.0',
            '--matcherTrafficServiceRouteWeight', '0.15',
            '--matcherMaxAssign', '1.0', '--matcherReassign', 'true',
            '--heapKeepBias', '0', '--heapProtectDist', '0'
        )
        Write-Host "$(Get-Date -Format o) Starting $stem"
        & $exe @arguments *> $log
        if ($LASTEXITCODE -ne 0) { throw "$stem exited with code $LASTEXITCODE; see $log" }
    }
    $result = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
    if ($result.numTaskFinished -le 0 -or $result.numEntryTimeouts -ne 0 -or
        $result.numPlannerErrors -ne 0 -or $result.numScheduleErrors -ne 0) {
        throw "Invalid result for $stem; see $output"
    }
    $metrics = Get-Content -LiteralPath (Join-Path $fullOutputDir "$stem.metrics_summary.json") -Raw | ConvertFrom-Json
    Write-Host "$(Get-Date -Format o) Completed ${stem}: $($result.numTaskFinished) tasks"
    [pscustomobject]@{
        Warehouse = $warehouse
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
$rows | Export-Csv -LiteralPath (Join-Path $fullOutputDir 'summary.csv') -NoTypeInformation -Encoding UTF8
