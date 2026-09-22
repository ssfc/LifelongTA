param(
    [int]$Timesteps = 1000,
    [string]$OutputRoot = "results/dormitory/sl8000_sparse_global_heap"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "build/lifelong.exe"
$instance = Join-Path $repo "instances\sortationLarge\sortationLarge_8000.json"
$outputDir = Join-Path $repo $OutputRoot
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"

$output = Join-Path $outputDir "sl8000_sparse-global-heap_t${Timesteps}.json"
$args = @(
    "--inputFile", $instance, "--output", $output, "--outputScreen", "3",
    "--simulationTime", "$Timesteps", "--planTimeLimit", "1000", "--preprocessTimeLimit", "30000",
    "--assignNew", "false", "--logDetailLevel", "2", "--scheduleModel", "7", "--useTraffic", "false",
    "--commitWindow", "1", "--matcherDistWeight", "10", "--matcherTopK", "100", "--matcherMaxMatrix", "2000000",
    "--matcherUseTraffic", "true", "--matcherTrafficTopK", "100", "--matcherTrafficCongestionWeight", "1.0",
    "--matcherTrafficServiceWeight", "0.0", "--matcherMaxAssign", "1.0", "--matcherReassign", "true",
    "--matcherScalable", "true", "--matcherScalableBucket", "8", "--matcherScalableCandidates", "2",
    "--matcherScalableGlobalHeap", "true", "--matcherScalableGlobalCandidates", "24"
)

& $exe @args 2>&1 | Tee-Object -FilePath ([System.IO.Path]::ChangeExtension($output, ".log"))
if ($LASTEXITCODE -ne 0) { throw "SL-8000 sparse global heap failed with exit code $LASTEXITCODE" }

$result = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
[pscustomobject]@{
    Method = "taskmatcher-sparse-global-heap"
    FinishedTasks = [int]$result.numTaskFinished
    EntryTimeouts = [int]$result.numEntryTimeouts
    PlannerErrors = [int]$result.numPlannerErrors
    ScheduleErrors = [int]$result.numScheduleErrors
} | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $outputDir "summary.csv")
