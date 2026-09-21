param(
    [int[]]$Seeds = @(0, 1, 2, 3, 4),
    [float[]]$Weights = @(0.0, 0.25, 0.5, 1.0),
    [string]$SourceRoot = "C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA",
    [string]$OutputDir = "results/dormitory/table2_f2_n80_contraflow_screen"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $SourceRoot "build-win\lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$out = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Weight-Tag([float]$Weight) {
    return ('cw{0:0.00}' -f $Weight).Replace('.', '')
}

function Invoke-Case([int]$Seed, [float]$Weight) {
    $tag = Weight-Tag $Weight
    $stem = "f2_n80_${tag}_seed$Seed"
    $output = Join-Path $out "$stem.json"
    if (Test-Path -LiteralPath $output) {
        try {
            $prior = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
            if ($prior.numTaskFinished -eq 500 -and (Test-Path -LiteralPath ($output -replace '\.json$', '.task_metrics.csv'))) { return }
        } catch {}
    }
    $args = @(
        '--inputFile', (Join-Path $SourceRoot "instances\problem_80_2_$Seed.json"),
        '--output', $output, '--outputScreen', '3', '--simulationTime', '400',
        '--planTimeLimit', '1000', '--preprocessTimeLimit', '30000', '--assignNew', 'false',
        '--commitWindow', '1', '--logDetailLevel', '3', '--scheduleModel', '7',
        '--useTraffic', 'false', '--matcherDistWeight', '10', '--matcherTopK', '100',
        '--matcherMaxMatrix', '2000000', '--matcherReassign', 'true', '--heapKeepBias', '0',
        '--heapProtectDist', '0', '--matcherUseTraffic', 'true', '--matcherTrafficTopK', '100',
        '--matcherTrafficCongestionWeight', '0', '--matcherTrafficContraflowWeight', $Weight
    )
    Write-Host "Running $stem"
    & $exe @args 2>&1 | Tee-Object -FilePath ($output -replace '\.json$', '.log')
    if ($LASTEXITCODE -ne 0) { throw "$stem failed with exit code $LASTEXITCODE" }
}

# Latin-square-like ordering: avoid giving one weight a systematic early/late
# machine state advantage while retaining one active simulator at a time.
foreach ($seed in $Seeds) {
    $offset = $seed % $Weights.Count
    for ($index = 0; $index -lt $Weights.Count; ++$index) {
        Invoke-Case $seed $Weights[($index + $offset) % $Weights.Count]
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
Write-Host "Counter-flow screen complete: $out"
