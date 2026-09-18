param(
    [int]$Timesteps = 1000,
    [int]$FlowTrafficTarget = 6089,
    [string]$OutputDir = "results/dormitory/taskmatcher_w500_k50_validation_windows"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "build/lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }

$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$fullOutputDir = Join-Path $repo $OutputDir
New-Item -ItemType Directory -Force -Path $fullOutputDir | Out-Null

function Invoke-ValidationRun([int]$Trial) {
    $label = "w500_traffic-k50_ratio100_t{0}_trial{1}" -f $Timesteps, $Trial
    $output = Join-Path $fullOutputDir "$label.json"
    if (Test-Path -LiteralPath $output) {
        try {
            if ((Get-Content -LiteralPath $output -Raw | ConvertFrom-Json).numTaskFinished -gt 0) {
                Write-Host "Skipping completed $label"
                return (Get-Content -LiteralPath $output -Raw | ConvertFrom-Json)
            }
        } catch {}
    }

    $log = [System.IO.Path]::ChangeExtension($output, ".log")
    $args = @(
        "--inputFile", (Join-Path $repo "instances/warehouseSmall/warehouseSmall_500.json"),
        "--output", $output, "--outputScreen", "3", "--simulationTime", "$Timesteps",
        "--planTimeLimit", "1000", "--preprocessTimeLimit", "30000", "--scheduleModel", "7",
        "--useTraffic", "false", "--assignNew", "false", "--commitWindow", "1", "--logDetailLevel", "2",
        "--matcherDistWeight", "10", "--matcherTopK", "100", "--matcherMaxMatrix", "2000000",
        "--matcherUseTraffic", "true", "--matcherTrafficTopK", "50", "--matcherMaxAssign", "1.0",
        "--matcherReassign", "true", "--heapKeepBias", "0", "--heapProtectDist", "0"
    )
    Write-Host "Running $label"
    & $exe @args 2>&1 | Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit code $LASTEXITCODE" }
    return (Get-Content -LiteralPath $output -Raw | ConvertFrom-Json)
}

# Run once first.  Additional trials are only useful if the candidate clears
# the same-machine Flow-Traffic baseline.
$first = Invoke-ValidationRun 1
if ([int]$first.numTaskFinished -ge $FlowTrafficTarget) {
    Invoke-ValidationRun 2 | Out-Null
    Invoke-ValidationRun 3 | Out-Null
} else {
    Write-Host "Trial 1 did not exceed Flow-Traffic target $FlowTrafficTarget; stopping after one trial."
}

$summary = Get-ChildItem -LiteralPath $fullOutputDir -Filter "w500_traffic-k50_ratio100_t${Timesteps}_trial*.json" |
    Sort-Object Name |
    ForEach-Object {
        $result = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        [pscustomobject]@{
            Trial = [int]([regex]::Match($_.BaseName, 'trial(\d+)$').Groups[1].Value)
            FinishedTasks = [int]$result.numTaskFinished
            EntryTimeouts = [int]$result.numEntryTimeouts
            PlannerErrors = [int]$result.numPlannerErrors
            ScheduleErrors = [int]$result.numScheduleErrors
            DeltaToFlowTraffic = [int]$result.numTaskFinished - $FlowTrafficTarget
        }
    }
$summary | Tee-Object -FilePath (Join-Path $fullOutputDir "summary.txt")
$summary | ConvertTo-Json | Set-Content (Join-Path $fullOutputDir "summary.json")
