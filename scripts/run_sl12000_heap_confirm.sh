#!/usr/bin/env bash
set -euo pipefail

# Long confirmation of the best SL-12000 GreedyHeap screen candidate.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/sl12000_heap_confirm"}
TIMESTEPS=${1:-1000}
OUTPUT="$OUT_DIR/sl12000_rebuild70_lns0_k250_t${TIMESTEPS}.json"
LOG="${OUTPUT%.json}.log"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"
if [[ -s "$OUTPUT" ]] && [[ $(jq -r '.numTaskFinished // 0' "$OUTPUT") -gt 0 ]]; then
  exit 0
fi

echo "Running SL12000 GreedyHeap rebuild70 lns0 topK250 for $TIMESTEPS steps"
"$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_12000.json" \
  --output "$OUTPUT" --outputScreen 3 --simulationTime "$TIMESTEPS" \
  --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
  --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
  --heapDistWeight 10 --heapMaxAssign 1.0 --heapReassign true \
  --heapKeepBias 6 --heapProtectDist 10 --heapSortK 250 \
  --heapRebuildPct 70 --heapLnsPct 0 >"$LOG" 2>&1
