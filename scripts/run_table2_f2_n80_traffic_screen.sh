#!/usr/bin/env bash
set -euo pipefail

# Paired five-seed screen of the lab traffic-aware TaskMatcher mechanism under
# the unchanged released Table 2 f=2, n=80 protocol.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_f2_n80_traffic_screen"

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
    taskmatcher_static|taskmatcher_traffic)
      extra=()
      [[ "$method" == taskmatcher_traffic ]] && extra=(--matcherUseTraffic true --matcherTrafficTopK 25 --matcherTrafficCongestionWeight 1)
      "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
        --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
        --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
        --matcherDistWeight 2 --matcherTaskLengthWeight 1 --matcherTopK 100 --matcherMaxMatrix 2000000 \
        --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 "${extra[@]}" >"$log" 2>&1
      ;;
    *) echo "Unknown method: $method" >&2; exit 2 ;;
  esac
}

for seed in $(seq 0 4); do
  for method in flow_unit taskmatcher_static taskmatcher_traffic; do
    run_one "$method" "$seed"
  done
done

printf '\a========== Table 2 f=2,n=80 traffic screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 traffic-aware TaskMatcher screening results are ready.\n' | wall 2>/dev/null || true
