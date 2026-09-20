#!/usr/bin/env bash
set -euo pipefail

# The planner keeps a guide trajectory and accumulates PIBT priority while an
# agent pursues an unchanged goal. Compare assignment-stability policies under
# the same lexicographic pickup-first objective.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_f2_n80_stability_screen"

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
    return
  fi
  extra=(--matcherReassign true)
  if [[ "$method" == locked ]]; then extra=(--matcherReassign false); fi
  if [[ "$method" == progress_locked ]]; then extra+=(--matcherLockAfterPickupProgress true); fi
  "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
    --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
    --matcherLexicographicPickup true --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --heapKeepBias 0 --heapProtectDist 0 "${extra[@]}" >"${output%.json}.log" 2>&1
}

for seed in $(seq 0 4); do
  for method in flow_unit free locked progress_locked; do run_one "$method" "$seed"; done
done

printf '\a========== Table 2 planner-consistent stability screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 planner-consistent stability screening results are ready.\n' | wall 2>/dev/null || true
