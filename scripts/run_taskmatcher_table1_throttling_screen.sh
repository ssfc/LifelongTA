#!/usr/bin/env bash
set -euo pipefail

# Screen assignment throttling on the three Table 1 Warehouse cells where
# Flow-Traffic exceeds the free-reassignment TaskMatcher configuration.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/portable_assignment/table1_taskmatcher_throttling_screen"
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local agents=$1 ratio=$2 ratio_tag=$3 output log
  output="$OUT_ROOT/warehouse-small_${agents}_ratio${ratio_tag}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  mkdir -p "$OUT_ROOT"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    return
  fi
  echo "Running TaskMatcher throttle n=$agents ratio=$ratio for $TIMESTEPS timesteps"
  "$BIN" --inputFile "$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 7 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --matcherDistWeight 10 --matcherMaxAssign "$ratio" --matcherTopK 100 \
    --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 >"$log" 2>&1
}

for agents in 400 500 600; do
  for spec in '0.2 20' '0.4 40' '0.6 60' '0.8 80' '1.0 100'; do
    read -r ratio ratio_tag <<<"$spec"
    run_one "$agents" "$ratio" "$ratio_tag"
  done
done

printf '\n\a========== Table 1 TaskMatcher throttling screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 1 TaskMatcher throttling screen has completed.\n' | wall 2>/dev/null || true
