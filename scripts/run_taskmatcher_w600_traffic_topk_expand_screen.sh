#!/usr/bin/env bash
set -euo pipefail

# Extend the W600 traffic candidate-count screen beyond the prior top-k=100.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/traffic_topk_w600_expand_screen"}
TIMESTEPS=${1:-100}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for top_k in 125 150 200; do
  output="$OUT_DIR/w600_k${top_k}_cw1p0_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    continue
  fi
  echo "Running W600 traffic top-k=$top_k"
  "$BIN" --inputFile "$ROOT/instances/warehouseSmall/warehouseSmall_600.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 7 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
    --matcherUseTraffic true --matcherTrafficTopK "$top_k" \
    --matcherTrafficCongestionWeight 1.0 >"$log" 2>&1
done

printf '\n\a========== W600 expanded traffic top-k screen complete ==========\n'
date '+Completed at %F %T %Z'
