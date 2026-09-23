#!/usr/bin/env bash
set -euo pipefail

# Screen the primary GreedyHeap pickup-distance weight on the local SL16 setup.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sl16_dist_weight_screen"
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 distance_weight=$2
  local output="$OUT_DIR/sortation-large_16000_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and .error == null' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s: heap-dist-weight=%s\n' "$(date '+%F %T')" "$label" "$distance_weight"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_16000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --heapDistWeight "$distance_weight" \
    --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 \
    --heapFlowZoneRows 20 --heapFlowZoneCols 10 --heapFlowPenaltyWeight 1.0 \
    --heapAgeBonus 0.0 --heapLocalExchangeTopK 0 \
    --heapTrafficRerankTopK 0 --heapTrafficRerankWeight 0.0 --logDetailLevel 2 >"$log" 2>&1
}

run_one w2 2.0
run_one w5 5.0
run_one w8 8.0
run_one w12 12.0

printf '\n\a========== SL16 distance-weight screen complete ==========\n'
date '+Completed at %F %T %Z'
wall 'SL16 distance-weight screen has completed.' 2>/dev/null || true
