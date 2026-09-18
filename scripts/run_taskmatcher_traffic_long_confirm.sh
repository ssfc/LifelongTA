#!/usr/bin/env bash
set -euo pipefail

# Long-horizon confirmation for the two traffic-aware TaskMatcher screen winners.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR="$ROOT/results/portable_assignment/traffic_long_confirm"
TIMESTEPS=${1:-1000}

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
  echo "Running W${agents} $label for $TIMESTEPS steps"
  "$BIN" --inputFile "$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 7 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 "$@" >"$log" 2>&1
}

run_one 500 static
run_one 500 traffic-k25 --matcherUseTraffic true --matcherTrafficTopK 25
run_one 600 static
run_one 600 traffic-k50 --matcherUseTraffic true --matcherTrafficTopK 50

printf '\n\a========== Traffic TaskMatcher long confirmation complete ==========\n'
date '+Completed at %F %T %Z'
