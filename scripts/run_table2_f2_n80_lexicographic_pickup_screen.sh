#!/usr/bin/env bash
set -euo pipefail

# Match Flow-Unit's pickup-distance objective exactly, then use task delivery
# length only to break equal-pickup-distance assignment ties.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_f2_n80_lexicographic_pickup_screen"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local method=$1
  local seed=$2
  local output="$OUT_ROOT/$method/${method}_${seed}.json"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then return; fi
  echo "[$(date '+%F %T %Z')] $method seed=$seed"
  if [[ "$method" == flow_unit ]]; then
    "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
      --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 1 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
      >"${output%.json}.log" 2>&1
  else
    "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
      --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
      --matcherLexicographicPickup true --matcherTopK 100 --matcherMaxMatrix 2000000 \
      --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 >"${output%.json}.log" 2>&1
  fi
}

for seed in $(seq 0 4); do
  run_one flow_unit "$seed"
  run_one lexicographic_pickup "$seed"
done

printf '\a========== Table 2 lexicographic pickup screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 lexicographic pickup screening results are ready.\n' | wall 2>/dev/null || true
