param(
    [int]$Seed = 0,
    [int]$Repeats = 5,
    [string]$SourceRoot = "C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA",
    [string]$OutputDir = "results/dormitory/table2_f2_n80_flow_repeatability"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $SourceRoot "build-win\lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$out = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $out | Out-Null

for ($repeat = 1; $repeat -le $Repeats; ++$repeat) {
    $output = Join-Path $out ("flow_unit_seed{0}_repeat{1}.json" -f $Seed, $repeat)
    $args = @(
        '--inputFile', (Join-Path $SourceRoot "instances\problem_80_2_$Seed.json"),
        '--output', $output, '--outputScreen', '3', '--simulationTime', '400',
        '--planTimeLimit', '1000', '--preprocessTimeLimit', '30000', '--assignNew', 'false',
        '--commitWindow', '1', '--logDetailLevel', '3', '--scheduleModel', '1', '--useTraffic', 'false'
    )
    Write-Host "Running Flow repeat $repeat/$Repeats for seed $Seed"
    & $exe @args 2>&1 | Tee-Object -FilePath ($output -replace '\.json$', '.log')
    if ($LASTEXITCODE -ne 0) { throw "repeat $repeat failed with exit code $LASTEXITCODE" }
}

Get-ChildItem -LiteralPath $out -Filter '*.json' | Sort-Object Name | ForEach-Object {
    $json = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
    [pscustomobject]@{ Run=$_.BaseName; Makespan=$json.makespan; Finished=$json.numTaskFinished
        EntryTimeouts=$json.numEntryTimeouts; PlannerErrors=$json.numPlannerErrors; ScheduleErrors=$json.numScheduleErrors }
} | Export-Csv -LiteralPath (Join-Path $out 'summary.csv') -NoTypeInformation
