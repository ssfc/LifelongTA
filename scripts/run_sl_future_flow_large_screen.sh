#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/sl_future_flow_large_screen"}
TIMESTEPS=${1:-100}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for agents in 16000 20000; do
  output="$OUT_DIR/sl${agents}_futureflow_fw0p01_noregret_lns10_t${TIMESTEPS}.json"
  log="$OUT_DIR/sl${agents}_futureflow_fw0p01_noregret_lns10_t${TIMESTEPS}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    continue
  fi

  echo "Running SL-$agents dynamic future-flow confirmation screen"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_${agents}.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --heapDistWeight 10 --heapMaxAssign 1.0 --heapReassign true \
    --heapKeepBias 6 --heapProtectDist 10 --heapSortK 250 \
    --heapRebuildPct 70 --heapLnsPct 10 \
    --heapFlowAwareSpatial true --heapSpatialCell 16 --heapSpatialCandidates 1000 \
    --heapTrafficWeight 0.25 --heapReassignMinGain 10 \
    --heapFutureFlowCell 32 --heapFutureFlowWeight 0.01 \
    --heapFutureFlowHardCap 0 --heapFutureFlowRegret false >"$log" 2>&1
done
