param(
    [int[]]$Seeds = @(2, 3, 4, 5, 6, 7, 8, 9),
    [string]$SourceRoot = "C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA",
    [string]$OutputDir = "results/dormitory/table2_f5_n100_contraflow_expand"
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $SourceRoot 'build-win\lifelong.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$out = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Invoke-Case([int]$Seed, [string]$Method) {
    $stem = "f5_n100_${Method}_seed$Seed"
    $output = Join-Path $out "$stem.json"
    if (Test-Path -LiteralPath $output) {
        try {
            $prior = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
            if ($prior.numTaskFinished -eq 500 -and (Test-Path -LiteralPath ($output -replace '\.json$', '.task_metrics.csv'))) { return }
        } catch {}
    }
    $args = @(
        '--inputFile', (Join-Path $SourceRoot "instances\problem_100_5_$Seed.json"),
        '--output', $output, '--outputScreen', '3', '--simulationTime', '400',
        '--planTimeLimit', '1000', '--preprocessTimeLimit', '30000', '--assignNew', 'false',
        '--commitWindow', '1', '--logDetailLevel', '3'
    )
    if ($Method -eq 'flow-unit') {
        $args += @('--scheduleModel', '1', '--useTraffic', 'false')
    } elseif ($Method -eq 'taskmatcher-cw050') {
        $args += @('--scheduleModel', '7', '--useTraffic', 'false', '--matcherDistWeight', '10',
            '--matcherTopK', '100', '--matcherMaxMatrix', '2000000', '--matcherReassign', 'true',
            '--heapKeepBias', '0', '--heapProtectDist', '0', '--matcherUseTraffic', 'true',
            '--matcherTrafficTopK', '100', '--matcherTrafficCongestionWeight', '0',
            '--matcherTrafficContraflowWeight', '0.5')
    } else { throw "Unknown method: $Method" }
    Write-Host "Running $stem"
    & $exe @args 2>&1 | Tee-Object -FilePath ($output -replace '\.json$', '.log')
    if ($LASTEXITCODE -ne 0) { throw "$stem failed with exit code $LASTEXITCODE" }
}

foreach ($seed in $Seeds) {
    # Alternate the method that runs first to reduce systematic machine-state drift.
    $methods = if (($seed % 2) -eq 0) { @('flow-unit', 'taskmatcher-cw050') } else { @('taskmatcher-cw050', 'flow-unit') }
    foreach ($method in $methods) { Invoke-Case $seed $method }
}

$rows = Get-ChildItem -LiteralPath $out -Filter '*.json' |
    Where-Object { $_.Name -notlike '*.metrics_summary.json' } |
    ForEach-Object {
        $json = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        [pscustomobject]@{ Run=$_.BaseName; Makespan=$json.makespan; Finished=$json.numTaskFinished;
            EntryTimeouts=$json.numEntryTimeouts; PlannerErrors=$json.numPlannerErrors; ScheduleErrors=$json.numScheduleErrors }
    }
$rows | Sort-Object Run | Export-Csv -LiteralPath (Join-Path $out 'summary.csv') -NoTypeInformation
Write-Host "f5,n100 counter-flow expansion complete: $out"
