param(
    [int]$Timesteps = 250,
    [string]$OutputDir = "results/dormitory/taskmatcher_w500_topk_screen_windows"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "build/lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }

$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$fullOutputDir = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $fullOutputDir | Out-Null

# The prior throttle screen established that max_assign_ratio < 1.0 reduces
# throughput.  Keep it at 1.0 and isolate the traffic candidate-pool size.
$topKs = @(10, 15, 25, 35, 50)
$runs = foreach ($topK in $topKs) {
    @{ Agents = 500; TrafficTopK = $topK; Ratio = "1.0" }
}

foreach ($run in $runs) {
    $label = "w{0}_traffic-k{1}_ratio100_t{2}" -f $run.Agents, $run.TrafficTopK, $Timesteps
    $output = Join-Path $fullOutputDir "$label.json"
    if (Test-Path -LiteralPath $output) {
        try {
            if ((Get-Content -LiteralPath $output -Raw | ConvertFrom-Json).numTaskFinished -gt 0) {
                Write-Host "Skipping completed $label"
                continue
            }
        } catch {}
    }

    $log = [System.IO.Path]::ChangeExtension($output, ".log")
    $input = Join-Path $repo "instances/warehouseSmall/warehouseSmall_500.json"
    $args = @(
        "--inputFile", $input, "--output", $output, "--outputScreen", "3",
        "--simulationTime", "$Timesteps", "--planTimeLimit", "1000",
        "--preprocessTimeLimit", "30000", "--scheduleModel", "7",
        "--useTraffic", "false", "--assignNew", "false", "--commitWindow", "1",
        "--logDetailLevel", "2", "--matcherDistWeight", "10", "--matcherTopK", "100",
        "--matcherMaxMatrix", "2000000", "--matcherUseTraffic", "true",
        "--matcherTrafficTopK", "$($run.TrafficTopK)", "--matcherMaxAssign", "$($run.Ratio)",
        "--matcherReassign", "true", "--heapKeepBias", "0", "--heapProtectDist", "0"
    )
    Write-Host "Running $label"
    & $exe @args 2>&1 | Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit code $LASTEXITCODE" }
}

$summary = foreach ($run in $runs) {
    $label = "w{0}_traffic-k{1}_ratio100_t{2}" -f $run.Agents, $run.TrafficTopK, $Timesteps
    $result = Get-Content -LiteralPath (Join-Path $fullOutputDir "$label.json") -Raw | ConvertFrom-Json
    [pscustomobject]@{
        Agents = $run.Agents; TrafficTopK = $run.TrafficTopK; AssignRatio = $run.Ratio
        FinishedTasks = $result.numTaskFinished; EntryTimeouts = $result.numEntryTimeouts
        PlannerErrors = $result.numPlannerErrors; ScheduleErrors = $result.numScheduleErrors
    }
}
$summary | Sort-Object FinishedTasks -Descending | Tee-Object -FilePath (Join-Path $fullOutputDir "summary.txt")
$summary | ConvertTo-Json | Set-Content (Join-Path $fullOutputDir "summary.json")
