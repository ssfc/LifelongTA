#!/usr/bin/env bash
set -euo pipefail

# Paired mechanism diagnostics for the single Table 2 cell where the 2:1
# TaskMatcher cost showed a five-seed improvement.  Every method uses the
# released f=2, n=80 streams, the same finite 500-task workload, and the
# released one-second planning budget.
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
  case "$method" in
    flow_unit)
      "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
        --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
        --scheduleModel 1 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
        >"$log" 2>&1
      ;;
    taskmatcher_10_1|taskmatcher_2_1)
      local pickup_weight=10
      [[ "$method" == taskmatcher_2_1 ]] && pickup_weight=2
      "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
        --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
        --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
        --matcherDistWeight "$pickup_weight" --matcherTaskLengthWeight 1 --matcherTopK 100 \
        --matcherMaxMatrix 2000000 --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
        >"$log" 2>&1
      ;;
    *) echo "Unknown method: $method" >&2; exit 2 ;;
  esac
}

for method in flow_unit taskmatcher_10_1 taskmatcher_2_1; do
  for seed in $(seq 0 4); do
    run_one "$method" "$seed"
  done
done

printf '\a========== Table 2 f=2,n=80 diagnostics complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 f=2,n=80 diagnostic metrics are ready.\n' | wall 2>/dev/null || true
