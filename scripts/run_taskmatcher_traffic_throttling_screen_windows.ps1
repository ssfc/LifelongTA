param(
    [int]$Timesteps = 250,
    [string]$OutputDir = "results/dormitory/traffic_throttling_screen_windows"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "build/lifelong.exe"
if (!(Test-Path $exe)) { throw "Missing executable: $exe" }

$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$fullOutputDir = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $fullOutputDir | Out-Null

$runs = @(
    @{ Agents = 500; TrafficTopK = 25; Ratio = "0.6"; RatioTag = "60" },
    @{ Agents = 600; TrafficTopK = 50; Ratio = "0.6"; RatioTag = "60" },
    @{ Agents = 500; TrafficTopK = 25; Ratio = "0.8"; RatioTag = "80" },
    @{ Agents = 600; TrafficTopK = 50; Ratio = "0.8"; RatioTag = "80" },
    @{ Agents = 500; TrafficTopK = 25; Ratio = "1.0"; RatioTag = "100" },
    @{ Agents = 600; TrafficTopK = 50; Ratio = "1.0"; RatioTag = "100" }
)

foreach ($run in $runs) {
    $label = "w{0}_traffic-k{1}_ratio{2}_t{3}" -f $run.Agents, $run.TrafficTopK, $run.RatioTag, $Timesteps
    $output = Join-Path $fullOutputDir "$label.json"
    if (Test-Path $output) {
        try {
            if ((Get-Content $output -Raw | ConvertFrom-Json).numTaskFinished -gt 0) {
                Write-Host "Skipping completed $label"
                continue
            }
        } catch {}
    }

    $log = [System.IO.Path]::ChangeExtension($output, ".log")
    $input = Join-Path $repo ("instances/warehouseSmall/warehouseSmall_{0}.json" -f $run.Agents)
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
    $label = "w{0}_traffic-k{1}_ratio{2}_t{3}" -f $run.Agents, $run.TrafficTopK, $run.RatioTag, $Timesteps
    $result = Get-Content (Join-Path $fullOutputDir "$label.json") -Raw | ConvertFrom-Json
    [pscustomobject]@{
        Agents = $run.Agents; TrafficTopK = $run.TrafficTopK; AssignRatio = $run.Ratio
        FinishedTasks = $result.numTaskFinished; EntryTimeouts = $result.numEntryTimeouts
        PlannerErrors = $result.numPlannerErrors; ScheduleErrors = $result.numScheduleErrors
    }
}
$summary | Tee-Object -FilePath (Join-Path $fullOutputDir "summary.txt")
$summary | ConvertTo-Json | Set-Content (Join-Path $fullOutputDir "summary.json")
