#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/sl12000_flow_aware_confirm"}
TIMESTEPS=${1:-1000}
OUTPUT="$OUT_DIR/sl12000_flowaware_tw0p25_t${TIMESTEPS}.json"
LOG="$OUT_DIR/sl12000_flowaware_tw0p25_t${TIMESTEPS}.log"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

if [[ -s "$OUTPUT" ]] && [[ $(jq -r '.numTaskFinished // 0' "$OUTPUT") -gt 0 ]]; then
  echo "Skipping completed output: $OUTPUT"
  exit 0
fi

"$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_12000.json" \
  --output "$OUTPUT" --outputScreen 3 --simulationTime "$TIMESTEPS" \
  --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
  --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
  --heapDistWeight 10 --heapMaxAssign 1.0 --heapReassign true \
  --heapKeepBias 6 --heapProtectDist 10 --heapSortK 250 \
  --heapRebuildPct 70 --heapLnsPct 10 \
  --heapFlowAwareSpatial true --heapSpatialCell 16 --heapSpatialCandidates 1000 \
  --heapTrafficWeight 0.25 --heapReassignMinGain 10 >"$LOG" 2>&1
