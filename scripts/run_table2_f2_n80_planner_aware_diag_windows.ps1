param(
    [int[]]$Seeds = @(0, 1, 2, 3, 4),
    [string]$SourceRoot = "C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA",
    [string]$OutputDir = "results/dormitory/table2_f2_n80_planner_aware_diag"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $SourceRoot "build-win\lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing diagnostic executable: $exe" }
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$out = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Invoke-Case([string]$Method, [int]$Seed) {
    $stem = "f2_n80_{0}_seed{1}" -f $Method, $Seed
    $output = Join-Path $out "$stem.json"
    if (Test-Path -LiteralPath $output) {
        try {
            $prior = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
            if ($prior.numTaskFinished -eq 500 -and
                (Test-Path -LiteralPath ($output -replace '\.json$', '.task_metrics.csv'))) { return }
        } catch {}
    }
    $args = @(
        '--inputFile', (Join-Path $SourceRoot "instances\problem_80_2_$Seed.json"),
        '--output', $output, '--outputScreen', '3', '--simulationTime', '400',
        '--planTimeLimit', '1000', '--preprocessTimeLimit', '30000', '--assignNew', 'false',
        '--commitWindow', '1', '--logDetailLevel', '3'
    )
    if ($Method -eq 'flow-unit') {
        $args += @('--scheduleModel', '1', '--useTraffic', 'false')
    } elseif ($Method -eq 'taskmatcher-free') {
        $args += @('--scheduleModel', '7', '--useTraffic', 'false', '--matcherDistWeight', '10',
            '--matcherTopK', '100', '--matcherMaxMatrix', '2000000', '--matcherReassign', 'true',
            '--heapKeepBias', '0', '--heapProtectDist', '0')
    } else { throw "Unknown method: $Method" }
    Write-Host "Running $stem"
    & $exe @args 2>&1 | Tee-Object -FilePath ($output -replace '\.json$', '.log')
    if ($LASTEXITCODE -ne 0) { throw "$stem failed with exit code $LASTEXITCODE" }
}

foreach ($seed in $Seeds) {
    Invoke-Case 'flow-unit' $seed
    Invoke-Case 'taskmatcher-free' $seed
}

$rows = Get-ChildItem -LiteralPath $out -Filter '*.json' |
    Where-Object { $_.Name -notlike '*.metrics_summary.json' } |
    ForEach-Object {
        $json = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        [pscustomobject]@{
            Run = $_.BaseName; Makespan = $json.makespan; Finished = $json.numTaskFinished
            EntryTimeouts = $json.numEntryTimeouts; PlannerErrors = $json.numPlannerErrors
            ScheduleErrors = $json.numScheduleErrors
        }
    }
$rows | Sort-Object Run | Export-Csv -LiteralPath (Join-Path $out 'summary.csv') -NoTypeInformation
Write-Host "Planner-aware f=2,n=80 diagnostic complete: $out"
