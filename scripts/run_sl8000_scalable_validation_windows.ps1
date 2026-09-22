param(
    [int]$Timesteps = 1000,
    [string]$OutputRoot = "results/dormitory/sl8000_scalable_validation"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "build/lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }

$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$outputDir = Join-Path $repo $OutputRoot
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$instance = Join-Path $repo "instances\sortationLarge\sortationLarge_8000.json"

function Invoke-Case([string]$Method) {
    $label = "sl8000_{0}_t{1}" -f $Method, $Timesteps
    $output = Join-Path $outputDir "$label.json"
    if (Test-Path -LiteralPath $output) {
        try {
            if ((Get-Content -LiteralPath $output -Raw | ConvertFrom-Json).numTaskFinished -gt 0) {
                Write-Host "Skipping completed $label"
                return
            }
        } catch {}
    }
    $args = @(
        "--inputFile", $instance, "--output", $output, "--outputScreen", "3",
        "--simulationTime", "$Timesteps", "--planTimeLimit", "1000", "--preprocessTimeLimit", "30000",
        "--assignNew", "false", "--logDetailLevel", "2"
    )
    if ($Method -eq "flow-traffic") {
        $args += @("--scheduleModel", "1", "--useTraffic", "true")
    } elseif ($Method -eq "taskmatcher-scalable-traffic-k100") {
        $args += @(
            "--scheduleModel", "7", "--useTraffic", "false", "--commitWindow", "1",
            "--matcherDistWeight", "10", "--matcherTopK", "100", "--matcherMaxMatrix", "2000000",
            "--matcherUseTraffic", "true", "--matcherTrafficTopK", "100", "--matcherTrafficCongestionWeight", "1.0",
            "--matcherTrafficServiceWeight", "0.0", "--matcherMaxAssign", "1.0", "--matcherReassign", "true",
            "--matcherScalable", "true", "--matcherScalableBucket", "8", "--matcherScalableCandidates", "2"
        )
    } else { throw "Unknown method: $Method" }
    Write-Host "Running $label"
    & $exe @args 2>&1 | Tee-Object -FilePath ([System.IO.Path]::ChangeExtension($output, ".log"))
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit code $LASTEXITCODE" }
}

Invoke-Case "flow-traffic"
Invoke-Case "taskmatcher-scalable-traffic-k100"

$rows = Get-ChildItem -LiteralPath $outputDir -Filter "sl8000_*_t${Timesteps}.json" |
    Where-Object { $_.Name -notlike "*.metrics_summary.json" } |
    Sort-Object Name | ForEach-Object {
        $result = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        [pscustomobject]@{
            Method = if ($_.BaseName -match "flow-traffic") { "flow-traffic" } else { "taskmatcher-scalable-traffic-k100" }
            FinishedTasks = [int]$result.numTaskFinished
            EntryTimeouts = [int]$result.numEntryTimeouts
            PlannerErrors = [int]$result.numPlannerErrors
            ScheduleErrors = [int]$result.numScheduleErrors
        }
    }
$rows | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $outputDir "summary.csv")
