#!/usr/bin/env bash
set -euo pipefail

# TaskMatcher stays a full Hungarian assignment.  Only each new task's first
# eligibility is delayed, allowing the scheduler to see a slightly larger pool.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_f2_n80_batch_delay_screen"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local method=$1 seed=$2 output log delay
  if [[ "$method" == flow_unit ]]; then
    output="$OUT_ROOT/flow_unit/flow_unit_${seed}.json"
    log="${output%.json}.log"
    mkdir -p "$(dirname "$output")"
    if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]] &&
       [[ -s "${output%.json}.metrics_summary.json" ]]; then
      return
    fi
    echo "[$(date '+%F %T %Z')] flow_unit seed=$seed"
    "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
      --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 1 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
      >"$log" 2>&1
    return
  fi
  delay=$method
  output="$OUT_ROOT/delay_${delay}/delay_${delay}_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]] &&
     [[ -s "${output%.json}.metrics_summary.json" ]]; then
    return
  fi
  echo "[$(date '+%F %T %Z')] delay=$delay seed=$seed"
  "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
    --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
    --matcherDistWeight 2 --matcherTaskLengthWeight 1 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
    --matcherNewTaskDelay "$delay" >"$log" 2>&1
}

for seed in $(seq 0 4); do
  for delay in 0 1 2; do
    run_one "$delay" "$seed"
  done
  run_one flow_unit "$seed"
done

printf '\a========== Table 2 TaskMatcher batch-delay screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 TaskMatcher batch-delay screening results are ready.\n' | wall 2>/dev/null || true
