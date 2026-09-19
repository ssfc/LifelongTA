#!/usr/bin/env bash
set -euo pipefail

# Paired W400 long runs with metrics sidecars for mechanism-level comparison.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR="$ROOT/results/metrics_compare/w400"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local name=$1
  shift
  local output="$OUT_DIR/w400_${name}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    return
  fi
  echo "Running W400 $name"
  "$BIN" --inputFile "$ROOT/instances/warehouseSmall/warehouseSmall_400.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --assignNew false \
    --commitWindow 1 --logDetailLevel 2 "$@" >"$log" 2>&1
}

run_one flow-traffic --scheduleModel 1 --useTraffic true
run_one taskmatcher-traffic-k100 --scheduleModel 7 --useTraffic false \
  --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
  --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
  --matcherUseTraffic true --matcherTrafficTopK 100 \
  --matcherTrafficCongestionWeight 1.0

printf '\n\a========== W400 metrics comparison complete ==========\n'
date '+Completed at %F %T %Z'
