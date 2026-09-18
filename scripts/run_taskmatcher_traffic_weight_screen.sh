#!/usr/bin/env bash
set -euo pipefail

# Local congestion-weight screen around the W500/W600 traffic winners.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR="$ROOT/results/portable_assignment/traffic_weight_screen"
TIMESTEPS=${1:-100}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local agents=$1 top_k=$2 weight=$3
  local tag=${weight/./p}
  local output="$OUT_DIR/w${agents}_k${top_k}_cw${tag}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    return
  fi
  echo "Running W${agents}: traffic top-k=$top_k, congestion weight=$weight"
  "$BIN" --inputFile "$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 7 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
    --matcherUseTraffic true --matcherTrafficTopK "$top_k" \
    --matcherTrafficCongestionWeight "$weight" >"$log" 2>&1
}

for weight in 0.75 1.0 1.25; do
  run_one 500 25 "$weight"
  run_one 600 50 "$weight"
done

printf '\n\a========== TaskMatcher traffic-weight screen complete ==========\n'
date '+Completed at %F %T %Z'
