#!/usr/bin/env bash
set -euo pipefail

# Complete the paired 25-seed validation for the only new Table 2 cost model
# with a favorable five-seed screen: TaskMatcher pickup:length weight 2:1.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_f2_n80_diagnostics"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local method=$1 seed=$2 output log
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
  else
    "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
      --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
      --matcherDistWeight 2 --matcherTaskLengthWeight 1 --matcherTopK 100 \
      --matcherMaxMatrix 2000000 --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
      >"$log" 2>&1
  fi
}

for seed in $(seq 5 24); do
  run_one flow_unit "$seed"
  run_one taskmatcher_2_1 "$seed"
done

printf '\a========== Table 2 f=2,n=80 25-seed validation complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 f=2,n=80 Flow-Unit versus TaskMatcher 2:1 validation is ready.\n' | wall 2>/dev/null || true
