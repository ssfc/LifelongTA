#!/usr/bin/env bash
set -euo pipefail

# Tune only the remaining Table 1 Sortation Large row below Flow-Traffic.
# The fixed planner/executor and all GreedyHeap settings are unchanged.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sl16_future_flow_tuning"
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 rows=$2 cols=$3 weight=$4
  local output="$OUT_DIR/sortation-large_16000_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and .error == null' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s: grid=%sx%s, flow-weight=%s\n' \
    "$(date '+%F %T')" "$label" "$rows" "$cols" "$weight"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_16000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --heapDistWeight 5 \
    --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 \
    --heapFlowZoneRows "$rows" --heapFlowZoneCols "$cols" \
    --heapFlowPenaltyWeight "$weight" --logDetailLevel 2 >"$log" 2>&1
}

# The 20x10, weight=1.0 setting is the validated current reference.
run_one reference_20x10_w100 20 10 1.0
run_one stronger_20x10_w150 20 10 1.5
run_one stronger_20x10_w200 20 10 2.0
run_one coarser_10x5_w100 10 5 1.0
run_one finer_30x15_w100 30 15 1.0

printf '\n\a========== SL16 future-flow tuning screen complete ==========\n'
date '+Completed at %F %T %Z'
wall 'SL16 future-flow tuning screen has completed.' 2>/dev/null || true
