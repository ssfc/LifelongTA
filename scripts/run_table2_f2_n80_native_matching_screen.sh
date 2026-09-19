#!/usr/bin/env bash
set -euo pipefail

# Screen the two released LifelongTA scheduler-only spatial matching models on
# the same Table 2 f=2, n=80 streams. Neither run enables traffic guide paths.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_f2_n80_native_matching_screen"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local method=$1 seed=$2 model output log
  case "$method" in
    flow_unit) model=1 ;;
    spatial_dijkstra) model=3 ;;
    cached_heuristic) model=4 ;;
    *) echo "Unknown method: $method" >&2; exit 2 ;;
  esac
  output="$OUT_ROOT/$method/${method}_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]] &&
     [[ -s "${output%.json}.metrics_summary.json" ]]; then
    return
  fi
  echo "[$(date '+%F %T %Z')] $method seed=$seed"
  "$BIN" --inputFile "$ROOT/instances/problem_80_2_${seed}.json" --output "$output" \
    --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel "$model" --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
    >"$log" 2>&1
}

for seed in $(seq 0 4); do
  for method in flow_unit spatial_dijkstra cached_heuristic; do
    run_one "$method" "$seed"
  done
done

printf '\a========== Table 2 native spatial matching screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 native spatial matching screening results are ready.\n' | wall 2>/dev/null || true
