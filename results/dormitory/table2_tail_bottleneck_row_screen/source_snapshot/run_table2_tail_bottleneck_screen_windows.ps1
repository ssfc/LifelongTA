param(
    [string[]]$Rows = @('f2_n50', 'f2_n80', 'f2_n100', 'f5_n50', 'f5_n80', 'f5_n100', 'f10_n50', 'f10_n80', 'f10_n100'),
    [int[]]$Seeds = @(0),
    [string]$SourceRoot = 'C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA',
    [string]$OutputDir = 'results/dormitory/table2_tail_bottleneck_row_screen',
    [switch]$ValidateOnly
)

# One new executable, identical shared planner, three schedulers. Never compare
# corrected telemetry with old batches as though they were paired observations.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path -Parent $PSScriptRoot
$buildExe = Join-Path $SourceRoot 'build-tail-bottleneck/lifelong.exe'
$out = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repo $OutputDir }
$out = [IO.Path]::GetFullPath($out)
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$methodNames = @('flow-unit', 'taskmatcher-free', 'taskmatcher-tail')
if (!(Test-Path -LiteralPath $buildExe)) { throw "Missing executable: $buildExe" }
if ($Rows.Count -eq 0 -or $Seeds.Count -eq 0) { throw 'Specify at least one row and seed.' }
if (@($Rows | Select-Object -Unique).Count -ne $Rows.Count -or @($Seeds | Select-Object -Unique).Count -ne $Seeds.Count) {
    throw 'Duplicate rows/seeds are not allowed.'
}
$inputFiles = @()
foreach ($row in $Rows) {
    if ($row -notmatch '^f(2|5|10)_n(50|80|100)$') { throw "Invalid Table 2 row: $row" }
    $release = [int]$Matches[1]; $agents = [int]$Matches[2]
    foreach ($seed in $Seeds) {
        if ($seed -lt 0 -or $seed -gt 24) { throw "Invalid seed: $seed" }
        $instancePath = Join-Path $SourceRoot "instances/problem_${agents}_${release}_$seed.json"
        $spec = Get-Content -LiteralPath $instancePath -Raw | ConvertFrom-Json
        $inputFiles += $instancePath
        foreach ($field in @('mapFile', 'agentFile', 'taskFile')) {
            $inputFiles += Join-Path (Split-Path -Parent $instancePath) $spec.$field
        }
    }
}
$inputs = @($inputFiles | Sort-Object -Unique | ForEach-Object {
    [ordered]@{ Path=[IO.Path]::GetFullPath($_); SHA256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash }
})
$expected = $Rows.Count * $Seeds.Count * $methodNames.Count
if ($ValidateOnly) {
    [pscustomobject]@{ Executable=$buildExe; Output=$out; Runs=$expected; Rows=$Rows; Seeds=$Seeds } | ConvertTo-Json
    return
}
if (@(Get-Process -Name lifelong -ErrorAction SilentlyContinue).Count -gt 0) {
    throw 'Another lifelong process is running; do not overlap timed experiments.'
}
New-Item -ItemType Directory -Force -Path $out | Out-Null
$manifestPath = Join-Path $out 'campaign_manifest.json'
$exe = Join-Path $out 'binary/lifelong.exe'
$configuration = [ordered]@{
    Format='table2-tail-screen-v1'; SourceRoot=$SourceRoot; Rows=$Rows; Seeds=$Seeds
    Methods=$methodNames; Tasks=500; SimulationSteps=400; PlanTimeLimitMs=1000
    PreprocessTimeLimitMs=30000; MatcherDistWeight=10; MatcherTaskLengthWeight=1
    MatcherUseTraffic=$false; MatcherReassign=$true; HeapKeepBias=0; HeapProtectDist=0
    Inputs=$inputs; RunnerSHA256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
    BuildSHA256=(Get-FileHash -LiteralPath $buildExe -Algorithm SHA256).Hash
}
$signature = $configuration | ConvertTo-Json -Depth 12 -Compress
if (Test-Path -LiteralPath $manifestPath) {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.Signature -cne $signature) { throw 'Campaign configuration changed; use a NEW output directory.' }
    if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $configuration.BuildSHA256) {
        throw 'Archived executable hash mismatch.'
    }
} else {
    if (@(Get-ChildItem -LiteralPath $out -Filter '*.json').Count -gt 0) { throw 'Results exist without a manifest.' }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $exe) | Out-Null
    Copy-Item -LiteralPath $buildExe -Destination $exe
    $snapshot = Join-Path $out 'source_snapshot'
    New-Item -ItemType Directory -Force -Path $snapshot | Out-Null
    foreach ($folder in @('src', 'inc', 'default_planner', 'tests', 'docs')) {
        $source = Join-Path $SourceRoot $folder
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $snapshot -Recurse }
    }
    Copy-Item -LiteralPath (Join-Path $SourceRoot 'CMakeLists.txt') -Destination $snapshot
    Copy-Item -LiteralPath $PSCommandPath -Destination $snapshot
    $manifest = [ordered]@{
        CreatedUtc=[DateTime]::UtcNow.ToString('o'); Host=$env:COMPUTERNAME
        SourceHead=(& git -C $SourceRoot rev-parse HEAD | Out-String).Trim()
        SourceStatus=@(& git -C $SourceRoot status --short)
        Signature=$signature; Configuration=$configuration
        Note='PIBT rollout disabled. Original shared planner retained, including wall-clock sensitivity. Screening, not statistical confirmation.'
    }
    $manifest | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
}

function Read-ValidatedResult([string]$Stem) {
    $path = Join-Path $out "$Stem.json"
    $j = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    foreach ($counter in @('numEntryTimeouts','numPlannerErrors','numScheduleErrors')) {
        if ($null -eq $j.$counter -or [int]$j.$counter -ne 0) { throw "$Stem has invalid $counter=$($j.$counter)" }
    }
    if ($j.numTaskFinished -ne 500 -or !$j.allTasksCompleted -or $null -eq $j.actualMakespan) {
        throw "$Stem did not finish all 500 tasks (cap=400)."
    }
    if ($j.makespan -ne $j.maxAgentActiveSteps -or $j.actualMakespan -ne $j.simulationSteps) {
        throw "$Stem has inconsistent completion counters."
    }
    $m = Get-Content -LiteralPath (Join-Path $out "$Stem.metrics_summary.json") -Raw | ConvertFrom-Json
    $tasks = @(Import-Csv -LiteralPath (Join-Path $out "$Stem.task_metrics.csv"))
    $agents = @(Import-Csv -LiteralPath (Join-Path $out "$Stem.agent_metrics.csv"))
    $timeline = @(Import-Csv -LiteralPath (Join-Path $out "$Stem.timeline_metrics.csv"))
    if ($tasks.Count -ne 500 -or @($tasks | Where-Object { [int]$_.completed_at -lt 0 }).Count -gt 0 -or
        ($tasks | Measure-Object -Property completed_at -Maximum).Maximum -ne $j.actualMakespan) {
        throw "$Stem task CSV disagrees with actualMakespan."
    }
    if ($timeline.Count -ne $j.simulationSteps -or $agents.Count -ne $j.teamSize) {
        throw "$Stem incomplete timeline/agent metrics."
    }
    foreach ($a in $agents) {
        if (([long]$a.idle_steps + [long]$a.empty_steps + [long]$a.loaded_steps) -ne $j.simulationSteps) {
            throw "$Stem agent $($a.agent_id) step accounting does not balance."
        }
    }
    $lastTen = @($tasks | Sort-Object { [int]$_.completed_at } -Descending | Select-Object -First 10)
    $decisions = @($j.tailBottleneckDecisions)
    [pscustomobject][ordered]@{
        Run=$Stem; ActualMakespan=$j.actualMakespan; ReportedMakespan=$j.makespan
        Finished=$j.numTaskFinished; EntryTimeouts=$j.numEntryTimeouts
        PlannerErrors=$j.numPlannerErrors; ScheduleErrors=$j.numScheduleErrors
        LastTenCompletionMean=($lastTen | Measure-Object -Property completed_at -Average).Average
        PickupDistance=$m.matchingQuality.pickupDistanceAtAssignment.mean
        EndToEndMean=$m.taskLatency.arrivalToCompletion.mean; EndToEndP95=$m.taskLatency.arrivalToCompletion.p95
        EmptyDistance=$m.movement.emptyDistance; LoadedDistance=$m.movement.loadedDistance
        LoadedDetourRatio=$m.movement.loadedDetourRatio.mean
        EmptyWait=($tasks | Measure-Object -Property empty_wait_steps -Sum).Sum
        LoadedWait=($tasks | Measure-Object -Property loaded_wait_steps -Sum).Sum
        Reassignments=($tasks | Measure-Object -Property reassignments -Sum).Sum
        PlannerSecondsMean=$m.plannerSeconds.mean; TailEligibleSteps=$decisions.Count
        TailAcceptedSteps=@($decisions | Where-Object { $_.accepted }).Count
    }
}

$collected = [Collections.Generic.List[object]]::new()
function Save-Progress([string]$State, [string]$CurrentRun='') {
    if ($collected.Count -gt 0) {
        $collected | Sort-Object Run | Export-Csv -LiteralPath (Join-Path $out 'summary.csv') -NoTypeInformation -Encoding UTF8
    }
    [ordered]@{ State=$State; CurrentRun=$CurrentRun; Completed=$collected.Count; Expected=$expected
        UpdatedUtc=[DateTime]::UtcNow.ToString('o'); RunnerPid=$PID } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'status.json') -Encoding UTF8
}

try {
    for ($rowIndex=0; $rowIndex -lt $Rows.Count; ++$rowIndex) {
        $row = $Rows[$rowIndex]
        $null = $row -match '^f(\d+)_n(\d+)$'
        $release = [int]$Matches[1]; $agentCount = [int]$Matches[2]
        foreach ($seed in $Seeds) {
            # Rotate the three methods across rows; there is no parallel running.
            for ($slot=0; $slot -lt $methodNames.Count; ++$slot) {
                $method = $methodNames[($rowIndex+$seed+$slot) % $methodNames.Count]
                $stem = "${row}_${method}_seed$seed"
                $output = Join-Path $out "$stem.json"
                Save-Progress 'running' $stem
                if (!(Test-Path -LiteralPath $output)) {
                    $arguments = @('--inputFile', (Join-Path $SourceRoot "instances/problem_${agentCount}_${release}_$seed.json"),
                        '--output', $output, '--outputScreen', '3', '--simulationTime', '400',
                        '--planTimeLimit', '1000', '--preprocessTimeLimit', '30000', '--assignNew', 'false',
                        '--commitWindow', '1', '--logDetailLevel', '3', '--logFile', (Join-Path $out "$stem.events.log"))
                    if ($method -eq 'flow-unit') {
                        $arguments += @('--scheduleModel', '1', '--useTraffic', 'false')
                    } else {
                        $tail = if ($method -eq 'taskmatcher-tail') { 'true' } else { 'false' }
                        $arguments += @('--scheduleModel', '7', '--useTraffic', 'false', '--matcherDistWeight', '10',
                            '--matcherTaskLengthWeight', '1', '--matcherTopK', '100', '--matcherMaxMatrix', '2000000',
                            '--matcherReassign', 'true', '--heapKeepBias', '0', '--heapProtectDist', '0',
                            '--matcherUseTraffic', 'false', '--matcherTailBottleneck', $tail)
                    }
                    [ordered]@{ Exe=$exe; SHA256=$configuration.BuildSHA256; Arguments=$arguments } |
                        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $out "$stem.command.json") -Encoding UTF8
                    Write-Host "Running $stem"
                    & $exe @arguments *> (Join-Path $out "$stem.log")
                    if ($LASTEXITCODE -ne 0) { throw "$stem exit code $LASTEXITCODE" }
                }
                # Existing incomplete/error results halt rather than being silently overwritten.
                $collected.Add((Read-ValidatedResult $stem))
                Save-Progress 'running' $stem
            }
        }
    }
    Save-Progress 'completed'
    Write-Host "Completed $expected runs: $out"
} catch {
    Save-Progress 'failed' $_.Exception.Message
    throw
}
