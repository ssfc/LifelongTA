#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/sl12000_flow_aware_screen"}
TIMESTEPS=${1:-100}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for traffic_weight in 0.25 1.0 2.0; do
  tag=${traffic_weight/./p}
  output="$OUT_DIR/sl12000_flowaware_tw${tag}_t${TIMESTEPS}.json"
  log="$OUT_DIR/sl12000_flowaware_tw${tag}_t${TIMESTEPS}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    continue
  fi

  echo "Running SL-12000 Flow-Aware Spatial GreedyHeap, traffic weight=$traffic_weight"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_12000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --heapDistWeight 10 --heapMaxAssign 1.0 --heapReassign true \
    --heapKeepBias 6 --heapProtectDist 10 --heapSortK 250 \
    --heapRebuildPct 70 --heapLnsPct 10 \
    --heapFlowAwareSpatial true --heapSpatialCell 16 --heapSpatialCandidates 1000 \
    --heapTrafficWeight "$traffic_weight" --heapReassignMinGain 10 >"$log" 2>&1
done
