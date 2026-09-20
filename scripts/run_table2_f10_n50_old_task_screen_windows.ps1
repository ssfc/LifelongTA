param(
    [string]$SourceRoot = "C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA",
    [string]$OutputDir = "results/dormitory/table2_f10_n50_old_task_screen",
    [int[]]$Seeds = @(8, 10, 11, 12, 15, 18, 23, 24)
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $SourceRoot "build-win\lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing Table 2 executable: $exe" }
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$fullOutputDir = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $fullOutputDir | Out-Null

# The first three variants isolate age protection. The fourth adds a small
# incumbent preference and near-pickup lock to test whether it further cuts
# reassignment-driven tail delay.
$variants = @(
    [pscustomobject]@{ Name = 'age_010'; AgeWeight = '0.10'; KeepBias = '0'; ProtectDistance = '0' },
    [pscustomobject]@{ Name = 'age_025'; AgeWeight = '0.25'; KeepBias = '0'; ProtectDistance = '0' },
    [pscustomobject]@{ Name = 'age_050'; AgeWeight = '0.50'; KeepBias = '0'; ProtectDistance = '0' },
    [pscustomobject]@{ Name = 'age_025_stable'; AgeWeight = '0.25'; KeepBias = '2'; ProtectDistance = '3' }
)

function Invoke-Case($Variant, [int]$Seed) {
    $label = "f10_n50_{0}_seed{1}" -f $Variant.Name, $Seed
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
        '--commitWindow', '1', '--logDetailLevel', '3', '--scheduleModel', '7', '--useTraffic', 'false',
        '--matcherDistWeight', '10', '--matcherTopK', '100', '--matcherMaxMatrix', '2000000',
        '--matcherReassign', 'true', '--matcherOldTaskAgeThreshold', '30',
        '--matcherOldTaskAgeWeight', $Variant.AgeWeight, '--heapKeepBias', $Variant.KeepBias,
        '--heapProtectDist', $Variant.ProtectDistance
    )
    Write-Host "Running $label"
    & $exe @args 2>&1 | Tee-Object -FilePath ($output -replace '\.json$', '.log')
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit code $LASTEXITCODE" }
}

foreach ($seed in $Seeds) {
    foreach ($variant in $variants) { Invoke-Case $variant $seed }
}

Get-ChildItem -LiteralPath $fullOutputDir -Filter '*.json' |
    Where-Object { $_.Name -notlike '*.metrics_summary.json' } |
    Sort-Object Name |
    ForEach-Object {
        $json = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        [pscustomobject]@{
            Run = $_.BaseName
            Makespan = $json.makespan
            Finished = $json.numTaskFinished
            EntryTimeouts = $json.numEntryTimeouts
            PlannerErrors = $json.numPlannerErrors
            ScheduleErrors = $json.numScheduleErrors
        }
    } | Export-Csv -LiteralPath (Join-Path $fullOutputDir 'summary.csv') -NoTypeInformation

Write-Host "Old-task protection screen complete: $fullOutputDir"
