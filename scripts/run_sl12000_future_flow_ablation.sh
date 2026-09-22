#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/sl12000_future_flow_ablation"}
TIMESTEPS=${1:-100}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_case() {
  local tag=$1 future_weight=$2 regret=$3 lns=$4
  local output="$OUT_DIR/sl12000_${tag}_t${TIMESTEPS}.json"
  local log="$OUT_DIR/sl12000_${tag}_t${TIMESTEPS}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    return
  fi

  echo "Running SL-12000 ablation: $tag"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_12000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --heapDistWeight 10 --heapMaxAssign 1.0 --heapReassign true \
    --heapKeepBias 6 --heapProtectDist 10 --heapSortK 250 \
    --heapRebuildPct 70 --heapLnsPct "$lns" \
    --heapFlowAwareSpatial true --heapSpatialCell 16 --heapSpatialCandidates 1000 \
    --heapTrafficWeight 0.25 --heapReassignMinGain 10 \
    --heapFutureFlowCell 32 --heapFutureFlowWeight "$future_weight" \
    --heapFutureFlowHardCap 0 --heapFutureFlowRegret "$regret" >"$log" 2>&1
}

run_case static_lns0 0 false 0
run_case future_fw0p001_noregret 0.001 false 0
run_case future_fw0p01_noregret 0.01 false 0
run_case future_fw0p01_noregret_lns10 0.01 false 10
