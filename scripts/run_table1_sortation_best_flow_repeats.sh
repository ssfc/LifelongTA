#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT_DIR="$ROOT/results/table1_sortation_best_flow_repeats"
mkdir -p "$OUT_DIR"

run_one() {
  local agents=$1 label=$2 model=$3 traffic=$4 rep=$5
  local output="$OUT_DIR/sortation-large_${agents}_${label}_rep${rep}_t1000.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e '.error == null and (.numTaskFinished | type) == "number"' "$output" >/dev/null; then
    printf 'Already complete: %s\n' "$output"
    return
  fi
  printf '[%s] SL%s %s repeat %s/3\n' "$(date '+%F %T')" "$agents" "$label" "$rep"
  "$ROOT/build/lifelong" \
    --inputFile "$ROOT/instances/sortationLarge/sortationLarge_${agents}.json" \
    --output "$output" --outputScreen 3 --simulationTime 1000 \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel "$model" \
    --useTraffic "$traffic" --assignNew false --commitWindow 1 \
    --logDetailLevel 2 >"$log" 2>&1
  jq -r '"finished=\(.numTaskFinished) entry_timeouts=\(.numEntryTimeouts) planner_errors=\(.numPlannerErrors) scheduler_errors=\(.numScheduleErrors)"' "$output"
}

for rep in 2 3; do
  run_one 4000 flow_unit 1 false "$rep"
  run_one 8000 flow_avg_waiting 2 false "$rep"
  run_one 12000 flow_avg_waiting 2 false "$rep"
  run_one 20000 flow_traffic 1 true "$rep"
done

printf '\n========== Sortation Large Flow repeats complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Sortation Large Flow repeats have completed.' 2>/dev/null || true
