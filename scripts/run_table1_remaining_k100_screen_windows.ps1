param(
    [int]$Timesteps = 1000,
    [string]$OutputRoot = "results/dormitory/table1_remaining_k100_screen"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo "build/lifelong.exe"
if (!(Test-Path -LiteralPath $exe)) { throw "Missing executable: $exe" }

$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$outputDir = Join-Path $repo $OutputRoot
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Case([string]$Family, [int]$Agents, [string]$Method) {
    $label = "{0}_{1}_{2}_t{3}" -f $Family, $Agents, $Method, $Timesteps
    $output = Join-Path $outputDir "$label.json"
    if (Test-Path -LiteralPath $output) {
        try {
            if ((Get-Content -LiteralPath $output -Raw | ConvertFrom-Json).numTaskFinished -gt 0) {
                Write-Host "Skipping completed $label"
                return
            }
        } catch {}
    }

    $instanceDir = if ($Family -eq "random") { "random" } else { "sortationLarge" }
    $instanceFile = Join-Path $repo ("instances/{0}/{1}_{2}.json" -f $instanceDir, $Family, $Agents)
    $args = @(
        "--inputFile", $instanceFile, "--output", $output, "--outputScreen", "3",
        "--simulationTime", "$Timesteps", "--planTimeLimit", "1000", "--preprocessTimeLimit", "30000",
        "--assignNew", "false", "--logDetailLevel", "2"
    )
    if ($Method -eq "flow-traffic") {
        $args += @("--scheduleModel", "1", "--useTraffic", "true")
    } elseif ($Method -eq "taskmatcher-traffic-k100") {
        $args += @(
            "--scheduleModel", "7", "--useTraffic", "false", "--commitWindow", "1",
            "--matcherDistWeight", "10", "--matcherTopK", "100", "--matcherMaxMatrix", "2000000",
            "--matcherUseTraffic", "true", "--matcherTrafficTopK", "100", "--matcherTrafficCongestionWeight", "1.0",
            "--matcherTrafficServiceWeight", "0.0", "--matcherMaxAssign", "1.0", "--matcherReassign", "true",
            "--heapKeepBias", "0", "--heapProtectDist", "0"
        )
    } else { throw "Unknown method: $Method" }

    Write-Host "Running $label"
    & $exe @args 2>&1 | Tee-Object -FilePath ([System.IO.Path]::ChangeExtension($output, ".log"))
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit code $LASTEXITCODE" }
}

$cases = @(
    @{ Family = "random"; Agents = 400 }, @{ Family = "random"; Agents = 800 },
    @{ Family = "random"; Agents = 1200 }, @{ Family = "random"; Agents = 1600 }, @{ Family = "random"; Agents = 2000 },
    @{ Family = "sortationLarge"; Agents = 4000 }, @{ Family = "sortationLarge"; Agents = 8000 },
    @{ Family = "sortationLarge"; Agents = 12000 }, @{ Family = "sortationLarge"; Agents = 16000 }, @{ Family = "sortationLarge"; Agents = 20000 }
)

foreach ($case in $cases) {
    Invoke-Case $case.Family $case.Agents "flow-traffic"
    Invoke-Case $case.Family $case.Agents "taskmatcher-traffic-k100"
}

$rows = Get-ChildItem -LiteralPath $outputDir -Filter "*_t${Timesteps}.json" |
    Where-Object { $_.Name -notlike "*.metrics_summary.json" } |
    Sort-Object Name | ForEach-Object {
        $result = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
        [pscustomobject]@{
            Name = $_.BaseName
            FinishedTasks = [int]$result.numTaskFinished
            EntryTimeouts = [int]$result.numEntryTimeouts
            PlannerErrors = [int]$result.numPlannerErrors
            ScheduleErrors = [int]$result.numScheduleErrors
        }
    }
$rows | Export-Csv -NoTypeInformation -Encoding utf8 -Path (Join-Path $outputDir "summary.csv")
