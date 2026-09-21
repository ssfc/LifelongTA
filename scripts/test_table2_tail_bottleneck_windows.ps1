param([string]$SourceRoot='C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=Split-Path -Parent $PSScriptRoot
$env:Path='C:\msys64\ucrt64\bin;'+$env:Path
$exe=Join-Path $SourceRoot 'build-tail-bottleneck/lifelong.exe'
$out=Join-Path $repo ('results/dormitory/tail_bottleneck_smoke_'+[DateTime]::Now.ToString('yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Force -Path $out | Out-Null
foreach($case in @('terminal-base','terminal-tail','streaming-tail','streaming-cutoff')) {
    $stem=$case.Split('-')[0]
    $tail=if($case -eq 'terminal-base'){'false'}else{'true'}
    $steps=if($case -eq 'streaming-cutoff'){'1'}else{'30'}
    $arguments=@('--inputFile',(Join-Path $repo "tests/tail_bottleneck_smoke/$stem.json"),
        '--output',(Join-Path $out "$case.json"),'--outputScreen','1','--simulationTime',$steps,
        '--planTimeLimit','1000','--preprocessTimeLimit','30000','--assignNew','false',
        '--commitWindow','1','--logDetailLevel','3','--logFile',(Join-Path $out "$case.events.log"),
        '--scheduleModel','7','--useTraffic','false','--matcherDistWeight','10',
        '--matcherTaskLengthWeight','1','--matcherTopK','100','--matcherMaxMatrix','2000000',
        '--matcherReassign','true','--heapKeepBias','0','--heapProtectDist','0',
        '--matcherUseTraffic','false','--matcherTailBottleneck',$tail)
    & $exe @arguments *> (Join-Path $out "$case.log")
    if($LASTEXITCODE -ne 0){throw "$case exit=$LASTEXITCODE"}
    $j=Get-Content -LiteralPath (Join-Path $out "$case.json") -Raw | ConvertFrom-Json
    foreach($field in @('numEntryTimeouts','numPlannerErrors','numScheduleErrors')){
        if($j.$field -ne 0){throw "$case $field=$($j.$field)"}
    }
    $tasks=@(Import-Csv -LiteralPath (Join-Path $out "$case.task_metrics.csv"))
    $agents=@(Import-Csv -LiteralPath (Join-Path $out "$case.agent_metrics.csv"))
    if($j.makespan -ne $j.maxAgentActiveSteps){throw 'Legacy alias mismatch'}
    if($case -eq 'streaming-cutoff'){
        if($j.allTasksCompleted -or $null -ne $j.actualMakespan){throw 'Censored run reported complete'}
    }else{
        if(!$j.allTasksCompleted -or $j.numTaskFinished -ne 2){throw "$case incomplete"}
        if(($tasks|Measure-Object completed_at -Maximum).Maximum -ne $j.actualMakespan){throw 'Completion mismatch'}
    }
    foreach($agent in $agents){
        if(([long]$agent.idle_steps+[long]$agent.empty_steps+[long]$agent.loaded_steps) -ne $j.simulationSteps){throw 'Step accounting mismatch'}
    }
    if($case -eq 'terminal-base' -and @($j.tailBottleneckDecisions).Count -ne 0){throw 'Disabled tail changed behavior'}
    if($case -eq 'terminal-tail' -and @($j.tailBottleneckDecisions|Where-Object {$_.accepted}).Count -eq 0){throw 'Expected improving fixture was not accepted'}
    foreach($decision in $j.tailBottleneckDecisions){
        if(!$decision.source_exhausted -or $decision.task_count -gt $decision.agent_count){throw 'Illegal activation'}
        if($stem -eq 'streaming' -and $decision.timestep -lt 1){throw 'Peeked at unreleased task'}
        if($decision.accepted -and !($decision.candidate_tail -lt $decision.baseline_tail -or
            ($decision.candidate_tail -eq $decision.baseline_tail -and $decision.candidate_pickup -lt $decision.baseline_pickup))){throw 'Nonimproving proposal accepted'}
    }
    [pscustomobject]@{Case=$case;Actual=$j.actualMakespan;Legacy=$j.makespan;Steps=$j.simulationSteps;Accepted=@($j.tailBottleneckDecisions|Where-Object {$_.accepted}).Count}
}
Write-Host "PASS: integrated tail/streaming/censored/telemetry smoke; $out"
