#!/usr/bin/env bash
set -euo pipefail

# Screen a Flow-Unit-seeded TaskMatcher. Flow supplies only a suggested edge;
# Hungarian remains the final scheduler and no guide paths are passed onward.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_f2_n80_flow_seed_screen"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local method=$1 seed=$2 output log bias
  output="$OUT_ROOT/$method/${method}_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]] &&
     [[ -s "${output%.json}.metrics_summary.json" ]]; then
    return
  fi
  echo "[$(date '+%F %T %Z')] $method seed=$seed"
  if [[ "$method" == flow_unit ]]; then
    "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
      --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 1 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
      >"$log" 2>&1
    return
  fi
  extra=()
  if [[ "$method" != static ]]; then
    bias=${method#flowseed_}
    extra=(--matcherFlowSeedBias "$bias" --matcherFlowSeedBudgetMs 100)
  fi
  "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
    --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
    --matcherDistWeight 2 --matcherTaskLengthWeight 1 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 "${extra[@]}" >"$log" 2>&1
}

for seed in $(seq 0 4); do
  for method in flow_unit static flowseed_1 flowseed_3 flowseed_6; do
    run_one "$method" "$seed"
  done
done

printf '\a========== Table 2 Flow-seeded TaskMatcher screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 Flow-seeded TaskMatcher screening results are ready.\n' | wall 2>/dev/null || true
