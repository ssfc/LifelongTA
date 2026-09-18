#!/usr/bin/env bash
set -euo pipefail

# Screen Flow-Traffic-compatible TaskMatcher costs only on the Warehouse rows
# where Flow-Traffic currently beats the static TaskMatcher.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR="$ROOT/results/portable_assignment/traffic_warehouse_screen"
TIMESTEPS=${1:-100}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local agents=$1 label=$2
  shift 2
  local output="$OUT_DIR/w${agents}_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    return
  fi
  echo "Running W${agents} $label"
  "$BIN" --inputFile "$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 7 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 "$@" >"$log" 2>&1
}

for agents in 400 500 600; do
  run_one "$agents" static
  run_one "$agents" traffic-k25 --matcherUseTraffic true --matcherTrafficTopK 25
  run_one "$agents" traffic-k50 --matcherUseTraffic true --matcherTrafficTopK 50
done

printf '\n\a========== Warehouse traffic TaskMatcher screen complete ==========\n'
date '+Completed at %F %T %Z'
