#!/usr/bin/env bash
set -euo pipefail

# Flow-Unit minimizes only distance to pickup. This screen tests whether a
# Hungarian assignment with that same objective, plus small delivery tie-breaks,
# can retain the Flow objective while improving finite-workload completion time.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_f2_n80_pickup_objective_screen"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_flow() {
  local seed=$1 output="$OUT_ROOT/flow_unit/flow_unit_${seed}.json"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then return; fi
  echo "[$(date '+%F %T %Z')] flow_unit seed=$seed"
  "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
    --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 1 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
    >"${output%.json}.log" 2>&1
}

run_matcher() {
  local tag=$1
  local length_weight=$2
  local seed=$3
  local output="$OUT_ROOT/$tag/${tag}_${seed}.json"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then return; fi
  echo "[$(date '+%F %T %Z')] $tag seed=$seed"
  "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
    --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
    --matcherDistWeight 1 --matcherTaskLengthWeight "$length_weight" \
    --matcherTopK 100 --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 >"${output%.json}.log" 2>&1
}

for seed in $(seq 0 4); do
  run_flow "$seed"
  run_matcher pickup_only 0 "$seed"
  run_matcher delivery_005 0.05 "$seed"
  run_matcher delivery_010 0.10 "$seed"
  run_matcher delivery_025 0.25 "$seed"
done

printf '\a========== Table 2 pickup-objective screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 pickup-objective screening results are ready.\n' | wall 2>/dev/null || true
