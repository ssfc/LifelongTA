param(
    [int]$Trials = 5,
    [string]$SourceRoot = "C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA",
    [string]$OutputDir = "results/dormitory/table2_f10_n50_mechanism_diag_exact"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $SourceRoot "build-win\lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing Table 2 executable: $exe" }
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$fullOutputDir = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $fullOutputDir | Out-Null

function Invoke-Case([string]$Method, [int]$Seed) {
    $label = "f10_n50_{0}_seed{1}" -f $Method, $Seed
    $output = Join-Path $fullOutputDir "$label.json"
    if (Test-Path -LiteralPath $output) {
        try {
            if ((Get-Content -LiteralPath $output -Raw | ConvertFrom-Json).numTaskFinished -eq 500 -and
                (Test-Path -LiteralPath ($output -replace '\.json$', '.metrics_summary.json'))) { return }
        } catch {}
    }
    $args = @(
        '--inputFile', (Join-Path $SourceRoot "instances\problem_50_10_$Seed.json"),
        '--output', $output, '--outputScreen', '3', '--simulationTime', '400',
        '--planTimeLimit', '1000', '--preprocessTimeLimit', '30000', '--assignNew', 'false',
        '--commitWindow', '1', '--logDetailLevel', '3'
    )
    if ($Method -eq 'flow-unit') {
        # Keep every Flow-Unit argument from dev-rmca-compare's paired campaign.
        $args += @('--scheduleModel', '1', '--useTraffic', 'false', '--heapDistWeight', '5',
            '--heapReassign', '1', '--heapKeepBias', '6', '--heapProtectDist', '10',
            '--heapRebuildPct', '45', '--heapLnsPct', '10', '--heapSortK', '500')
    } elseif ($Method -eq 'taskmatcher-free') {
        $args += @('--scheduleModel', '7', '--useTraffic', 'false', '--matcherDistWeight', '10',
            '--matcherTopK', '100', '--matcherMaxMatrix', '2000000', '--matcherReassign', 'true',
            '--heapKeepBias', '0', '--heapProtectDist', '0')
    } else { throw "Unknown method: $Method" }
    Write-Host "Running $label"
    & $exe @args 2>&1 | Tee-Object -FilePath ($output -replace '\.json$', '.log')
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit code $LASTEXITCODE" }
}

for ($seed = 0; $seed -lt $Trials; ++$seed) {
    Invoke-Case 'flow-unit' $seed
    Invoke-Case 'taskmatcher-free' $seed
}
