param(
    [string[]]$Rows = @('f2_n50', 'f2_n80', 'f2_n100', 'f5_n50', 'f5_n80', 'f5_n100', 'f10_n50', 'f10_n80', 'f10_n100'),
    [int[]]$Seeds = @(0, 1),
    [string]$SourceRoot = "C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA",
    [string]$OutputDir = "results/dormitory/table2_contraflow_row_screen"
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $SourceRoot 'build-win\lifelong.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$out = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Invoke-Case([string]$Row, [int]$Release, [int]$Agents, [int]$Seed, [string]$Method) {
    $stem = "${Row}_${Method}_seed$Seed"
    $output = Join-Path $out "$stem.json"
    if (Test-Path -LiteralPath $output) {
        try {
            $prior = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
            if ($prior.numTaskFinished -eq 500 -and (Test-Path -LiteralPath ($output -replace '\.json$', '.task_metrics.csv'))) { return }
        } catch {}
    }
    $args = @(
        '--inputFile', (Join-Path $SourceRoot "instances\problem_${Agents}_${Release}_$Seed.json"),
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

for ($rowIndex = 0; $rowIndex -lt $Rows.Count; ++$rowIndex) {
    $row = $Rows[$rowIndex]
    if ($row -notmatch '^f(\d+)_n(\d+)$') { throw "Invalid Table 2 row: $row" }
    $release = [int]$Matches[1]
    $agents = [int]$Matches[2]
    foreach ($seed in $Seeds) {
        # Alternate within every row/seed pair to limit systematic warm-up bias.
        $methods = if ((($rowIndex + $seed) % 2) -eq 0) {
            @('flow-unit', 'taskmatcher-cw050')
        } else { @('taskmatcher-cw050', 'flow-unit') }
        foreach ($method in $methods) { Invoke-Case $row $release $agents $seed $method }
    }
}

$rows = Get-ChildItem -LiteralPath $out -Filter '*.json' |
    Where-Object { $_.Name -notlike '*.metrics_summary.json' } |
    ForEach-Object {
        $json = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        [pscustomobject]@{ Run=$_.BaseName; Makespan=$json.makespan; Finished=$json.numTaskFinished;
            EntryTimeouts=$json.numEntryTimeouts; PlannerErrors=$json.numPlannerErrors; ScheduleErrors=$json.numScheduleErrors }
    }
$rows | Sort-Object Run | Export-Csv -LiteralPath (Join-Path $out 'summary.csv') -NoTypeInformation
Write-Host "Table 2 counter-flow row screen complete: $out"
