param(
    [string]$ResultsRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'results'),
    [string]$OutputRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'results/table2_actual_makespan_audit')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Get-ActualMakespanRecord([System.IO.FileInfo]$JsonFile) {
    $taskCsv = $JsonFile.FullName -replace '\.json$', '.task_metrics.csv'
    $json = Get-Content -LiteralPath $JsonFile.FullName -Raw | ConvertFrom-Json
    $record = [ordered]@{
        ResultFile = $JsonFile.FullName
        ResultRoot = Split-Path -Leaf (Split-Path -Parent $JsonFile.FullName)
        LegacyMakespan = [int]$json.makespan
        FinishedTasks = [int]$json.numTaskFinished
        TaskMetricFile = $taskCsv
        TaskRows = 0
        UnfinishedTasks = $null
        ActualMakespan = $null
        AllTasksCompleted = $false
        DeltaActualMinusLegacy = $null
        PlannerErrors = [int]$json.numPlannerErrors
        ScheduleErrors = [int]$json.numScheduleErrors
        EntryTimeouts = [int]$json.numEntryTimeouts
        AuditStatus = 'missing-task-metrics'
    }

    if (-not (Test-Path -LiteralPath $taskCsv)) { return [pscustomobject]$record }
    $tasks = @(Import-Csv -LiteralPath $taskCsv)
    $record.TaskRows = $tasks.Count
    if ($tasks.Count -eq 0 -or $null -eq $tasks[0].completed_at) {
        $record.AuditStatus = 'invalid-task-metrics'
        return [pscustomobject]$record
    }

    $unfinished = @($tasks | Where-Object { [int]$_.completed_at -lt 0 })
    $record.UnfinishedTasks = $unfinished.Count
    if ($unfinished.Count -gt 0 -or $record.FinishedTasks -ne $tasks.Count) {
        $record.AuditStatus = 'censored-or-incomplete'
        return [pscustomobject]$record
    }

    $actual = [int](($tasks | Measure-Object -Property completed_at -Maximum).Maximum)
    $record.ActualMakespan = $actual
    $record.AllTasksCompleted = $true
    $record.DeltaActualMinusLegacy = $actual - $record.LegacyMakespan
    $record.AuditStatus = if ($record.PlannerErrors -eq 0 -and $record.ScheduleErrors -eq 0 -and
                              $record.EntryTimeouts -eq 0) { 'valid' } else { 'completed-with-errors' }
    return [pscustomobject]$record
}

function Get-Table2Coordinates([string]$Path) {
    if ($Path -match '[\\/]f(?<f>2|5|10)_n(?<n>50|80|100)[\\/]') {
        return [pscustomobject]@{ Frequency = [int]$Matches.f; Agents = [int]$Matches.n }
    }
    return $null
}

function Get-Seed([string]$Path) {
    if ($Path -match '(?:seed_|flow_unit_|taskmatcher_fixed_)(?<seed>\d+)\.json$') { return [int]$Matches.seed }
    return $null
}

$normalizedOutputRoot = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\')
$resultFiles = @(Get-ChildItem -LiteralPath $ResultsRoot -Recurse -File -Filter '*.json' |
    Where-Object {
        -not $_.FullName.StartsWith($normalizedOutputRoot, [StringComparison]::OrdinalIgnoreCase) -and
        $_.FullName -match '[\\/]table2' -and $_.Name -notlike '*.metrics_summary.json'
    })

$records = @($resultFiles | ForEach-Object { Get-ActualMakespanRecord $_ } |
    Sort-Object ResultFile)
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot 'actual_makespan_records.csv') -NoTypeInformation -Encoding utf8
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputRoot 'actual_makespan_records.json') -Encoding utf8

# Pair every valid candidate with the same finite-stream Flow-Unit input/seed.
# This preserves all parameter screens as separate candidates instead of silently
# selecting the strongest result after seeing its outcome.
$byPath = @{}
foreach ($record in $records) { $byPath[$record.ResultFile] = $record }
$pairs = @()
foreach ($record in $records) {
    if ($record.AuditStatus -ne 'valid') { continue }
    $coords = Get-Table2Coordinates $record.ResultFile
    $seed = Get-Seed $record.ResultFile
    if ($null -eq $coords -or $null -eq $seed) { continue }
    if ($record.ResultFile -match '[\\/]table2_f2_n50_flow_unit[\\/]') { continue }
    if ($record.ResultFile -match '[\\/]table2_fixed_paired[\\/]f\d+_n\d+[\\/]flow_unit_') { continue }

    if ($coords.Frequency -eq 2 -and $coords.Agents -eq 50) {
        $flowPath = Join-Path $ResultsRoot "table2_f2_n50_flow_unit/flow_unit_$seed.json"
    } else {
        $flowPath = Join-Path $ResultsRoot "table2_fixed_paired/f$($coords.Frequency)_n$($coords.Agents)/flow_unit_$seed.json"
    }
    if (-not $byPath.ContainsKey($flowPath)) { continue }
    $flow = $byPath[$flowPath]
    if ($flow.AuditStatus -ne 'valid') { continue }

    $pairs += [pscustomobject][ordered]@{
        CandidateFile = $record.ResultFile
        FlowFile = $flow.ResultFile
        Frequency = $coords.Frequency
        Agents = $coords.Agents
        Seed = $seed
        CandidateActualMakespan = $record.ActualMakespan
        FlowActualMakespan = $flow.ActualMakespan
        DeltaCandidateMinusFlow = $record.ActualMakespan - $flow.ActualMakespan
        CandidateLegacyMakespan = $record.LegacyMakespan
        FlowLegacyMakespan = $flow.LegacyMakespan
    }
}
$pairs | Sort-Object CandidateFile | Export-Csv -LiteralPath (Join-Path $OutputRoot 'actual_makespan_pairs.csv') -NoTypeInformation -Encoding utf8

$pairSummary = @($pairs | Group-Object {
    $relative = $_.CandidateFile.Substring($ResultsRoot.TrimEnd('\').Length).TrimStart('\')
    $experiment = ($relative -split '[\\/]')[0]
    "$experiment|f$($_.Frequency)|n$($_.Agents)"
} | ForEach-Object {
    $group = @($_.Group)
    $deltas = @($group | ForEach-Object { [double]$_.DeltaCandidateMinusFlow })
    $parts = $_.Name -split '\|'
    [pscustomobject][ordered]@{
        Experiment = $parts[0]
        Frequency = [int](($parts[1] -replace '^f', ''))
        Agents = [int](($parts[2] -replace '^n', ''))
        Pairs = $group.Count
        CandidateActualMean = [math]::Round((($group | Measure-Object CandidateActualMakespan -Average).Average), 3)
        FlowActualMean = [math]::Round((($group | Measure-Object FlowActualMakespan -Average).Average), 3)
        MeanDeltaCandidateMinusFlow = [math]::Round((($deltas | Measure-Object -Average).Average), 3)
        Wins = @($deltas | Where-Object { $_ -lt 0 }).Count
        Ties = @($deltas | Where-Object { $_ -eq 0 }).Count
        Losses = @($deltas | Where-Object { $_ -gt 0 }).Count
    }
})
$pairSummary | Sort-Object Experiment, Frequency, Agents |
    Export-Csv -LiteralPath (Join-Path $OutputRoot 'actual_makespan_pair_summary.csv') -NoTypeInformation -Encoding utf8

$summary = [ordered]@{
    generatedAt = (Get-Date).ToString('s')
    sourceResultsRoot = $ResultsRoot
    resultFilesAudited = $records.Count
    validCompletedRuns = @($records | Where-Object AuditStatus -eq 'valid').Count
    incompleteOrCensoredRuns = @($records | Where-Object AuditStatus -eq 'censored-or-incomplete').Count
    missingOrInvalidMetrics = @($records | Where-Object { $_.AuditStatus -match 'missing|invalid' }).Count
    runsWithDifferentLegacyMakespan = @($records | Where-Object {
        $_.AuditStatus -eq 'valid' -and $_.DeltaActualMinusLegacy -ne 0
    }).Count
    pairedComparisons = $pairs.Count
    pairedComparisonGroups = $pairSummary.Count
}
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputRoot 'audit_summary.json') -Encoding utf8
$summary | Format-List
