#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/sl12000_future_flow_screen"}
TIMESTEPS=${1:-100}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for future_weight in 0.01 0.05 0.10; do
  tag=${future_weight/./p}
  output="$OUT_DIR/sl12000_futureflow_fw${tag}_t${TIMESTEPS}.json"
  log="$OUT_DIR/sl12000_futureflow_fw${tag}_t${TIMESTEPS}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    continue
  fi

  echo "Running SL-12000 dynamic zone future-flow weight=$future_weight"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_12000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --heapDistWeight 10 --heapMaxAssign 1.0 --heapReassign true \
    --heapKeepBias 6 --heapProtectDist 10 --heapSortK 250 \
    --heapRebuildPct 70 --heapLnsPct 0 \
    --heapFlowAwareSpatial true --heapSpatialCell 16 --heapSpatialCandidates 1000 \
    --heapTrafficWeight 0.25 --heapReassignMinGain 10 \
    --heapFutureFlowCell 32 --heapFutureFlowWeight "$future_weight" \
    --heapFutureFlowHardCap 0 --heapFutureFlowRegret true >"$log" 2>&1
done
