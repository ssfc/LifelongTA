param(
    [int]$Timesteps = 1000,
    [string]$OutputDir = "results/lab/warehouse-small_200_strict_compare"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "build-ucrt/lifelong.exe"
$input = Join-Path $repo "instances/warehouseSmall/warehouseSmall_200.json"
if (!(Test-Path $exe)) { throw "Missing executable: $exe" }
if (!(Test-Path $input)) { throw "Missing instance: $input" }

# The UCRT build depends on the MSYS UCRT runtime DLLs on Windows.
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$fullOutputDir = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $fullOutputDir | Out-Null

$methods = @(
    @{ Name = "greedy"; ScheduleModel = 5; UseTraffic = "false"; Extra = @() },
    @{ Name = "flow-unit"; ScheduleModel = 1; UseTraffic = "false"; Extra = @() },
    @{ Name = "flow-traffic"; ScheduleModel = 1; UseTraffic = "true"; Extra = @() },
    @{ Name = "flow-avg-waiting"; ScheduleModel = 2; UseTraffic = "false"; Extra = @() },
    @{ Name = "portable-greedy-heap"; ScheduleModel = 6; UseTraffic = "false"; Extra = @("--heapDistWeight", "5", "--heapKeepBias", "6") }
)

foreach ($method in $methods) {
    $output = Join-Path $fullOutputDir ("warehouse-small_200_{0}_{1}.json" -f $method.Name, $Timesteps)
    $log = [System.IO.Path]::ChangeExtension($output, ".log")
    if (Test-Path $output) {
        Write-Host "Skipping completed $($method.Name)"
        continue
    }
    $args = @(
        "--inputFile", $input,
        "--output", $output,
        "--outputScreen", "3",
        "--simulationTime", "$Timesteps",
        "--planTimeLimit", "1000",
        "--preprocessTimeLimit", "30000",
        "--scheduleModel", "$($method.ScheduleModel)",
        "--useTraffic", "$($method.UseTraffic)",
        "--assignNew", "false",
        "--logDetailLevel", "3"
    ) + $method.Extra

    Write-Host "Running $($method.Name)"
    & $exe @args 2>&1 | Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0) {
        throw "$($method.Name) failed with exit code $LASTEXITCODE"
    }
}

$rows = foreach ($method in $methods) {
    $path = Join-Path $fullOutputDir ("warehouse-small_200_{0}_{1}.json" -f $method.Name, $Timesteps)
    $result = Get-Content $path -Raw | ConvertFrom-Json
    $times = @($result.plannerTimes)
    [pscustomobject]@{
        Method = $method.Name
        FinishedTasks = $result.numTaskFinished
        EntryTimeouts = $result.numEntryTimeouts
        PlannerMeanSeconds = [math]::Round((($times | Measure-Object -Average).Average), 6)
        PlannerMaxSeconds = [math]::Round((($times | Measure-Object -Maximum).Maximum), 6)
    }
}
$rows | Sort-Object Method | Format-Table -AutoSize
$rows | ConvertTo-Json | Set-Content (Join-Path $fullOutputDir "summary.json")
