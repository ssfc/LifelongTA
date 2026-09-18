#!/usr/bin/env bash
set -euo pipefail

# Long confirmation of the W600 congestion-weight screen winner.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR="$ROOT/results/portable_assignment/traffic_weight_confirm"
TIMESTEPS=${1:-1000}
OUTPUT="$OUT_DIR/w600_k50_cw1p25_t${TIMESTEPS}.json"
LOG="${OUTPUT%.json}.log"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"
if [[ -s "$OUTPUT" ]] && [[ $(jq -r '.numTaskFinished // 0' "$OUTPUT") -gt 0 ]]; then
  exit 0
fi

echo "Running W600 traffic-k50 congestion-weight=1.25 for $TIMESTEPS steps"
"$BIN" --inputFile "$ROOT/instances/warehouseSmall/warehouseSmall_600.json" \
  --output "$OUTPUT" --outputScreen 3 --simulationTime "$TIMESTEPS" \
  --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 7 \
  --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
  --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
  --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
  --matcherUseTraffic true --matcherTrafficTopK 50 \
  --matcherTrafficCongestionWeight 1.25 >"$LOG" 2>&1

printf '\n\a========== W600 traffic weight 1.25 confirmation complete ==========\n'
date '+Completed at %F %T %Z'
