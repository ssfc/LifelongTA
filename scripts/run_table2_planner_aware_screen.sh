#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
INPUT_ROOT=${INPUT_ROOT:-"$ROOT/instances/table2"}
OUT_ROOT="$ROOT/results/table2_planner_aware_screen"
TIMESTEPS=${1:-400}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_ROOT"

for f in 2 5 10; do
  for n in 50 80 100; do
    for seed in 0 1 2; do
      output="$OUT_ROOT/f${f}_n${n}/seed_${seed}.json"
      log="${output%.json}.log"
      mkdir -p "$(dirname "$output")"
      if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]] &&
         [[ $(jq -r '.numPlannerErrors // -1' "$output") -eq 0 ]] &&
         [[ $(jq -r '.numScheduleErrors // -1' "$output") -eq 0 ]] &&
         [[ $(jq -r '.numEntryTimeouts // -1' "$output") -eq 0 ]]; then
        continue
      fi

      "$BIN" --inputFile "$INPUT_ROOT/problem_${n}_${f}_${seed}.json" --output "$output" --outputScreen 3 \
        --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
        --scheduleModel 7 --useTraffic false --assignNew false --finiteTaskStream true --commitWindow 1 \
        --logDetailLevel 2 --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
        --matcherMaxAssign 0.5 --matcherAdaptiveAssign true --matcherReassign true \
        --matcherUseTraffic true --matcherTrafficTopK 100 --matcherTrafficCongestionWeight 1.0 \
        --matcherUseWaitHeat true --matcherWaitHeatWeight 0.5 --matcherWaitHeatPressure 1.0 \
        --heapKeepBias 0 --heapProtectDist 0 >"$log" 2>&1
    done
  done
done
