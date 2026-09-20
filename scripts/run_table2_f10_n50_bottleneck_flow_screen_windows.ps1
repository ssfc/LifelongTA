param(
    [string]$SourceRoot = "C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA",
    [string]$OutputDir = "results/dormitory/table2_f10_n50_bottleneck_flow_screen",
    [int[]]$Seeds = @(8, 10, 11, 12, 15, 18, 23, 24)
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $SourceRoot "build-win\lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing Table 2 executable: $exe" }
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$fullOutputDir = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $fullOutputDir | Out-Null

# Capacity is the number of predicted routes a Kiva cell may absorb without a
# penalty. Only the marginal squared overload is charged afterwards.
$variants = @(
    [pscustomobject]@{ Name = 'cap2_w010'; Capacity = '2'; Weight = '0.10' },
    [pscustomobject]@{ Name = 'cap2_w025'; Capacity = '2'; Weight = '0.25' },
    [pscustomobject]@{ Name = 'cap3_w025'; Capacity = '3'; Weight = '0.25' },
    [pscustomobject]@{ Name = 'cap3_w050'; Capacity = '3'; Weight = '0.50' }
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
        '--matcherReassign', 'true', '--heapKeepBias', '0', '--heapProtectDist', '0',
        '--matcherUseBottleneckFlow', 'true', '--matcherBottleneckCapacity', $Variant.Capacity,
        '--matcherBottleneckPenaltyWeight', $Variant.Weight, '--matcherBottleneckIterations', '2',
        '--matcherProjectedTopK', '100'
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

Write-Host "Kiva bottleneck-flow screen complete: $fullOutputDir"
