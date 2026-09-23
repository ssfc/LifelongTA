#!/usr/bin/env bash
set -euo pipefail

# Measure run-to-run variance before promoting a short-horizon tuning result.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sl16_future_flow_repeats"
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 rows=$2 cols=$3 weight=$4 rep=$5
  local output="$OUT_DIR/sortation-large_16000_${label}_rep${rep}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and .error == null' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s rep=%s: grid=%sx%s, weight=%s\n' \
    "$(date '+%F %T')" "$label" "$rep" "$rows" "$cols" "$weight"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_16000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --heapDistWeight 5 \
    --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 \
    --heapFlowZoneRows "$rows" --heapFlowZoneCols "$cols" \
    --heapFlowPenaltyWeight "$weight" --logDetailLevel 2 >"$log" 2>&1
}

for rep in 1 2 3; do
  run_one baseline 20 10 0.0 "$rep"
  run_one flow20x10_w100 20 10 1.0 "$rep"
  run_one flow30x15_w100 30 15 1.0 "$rep"
done

printf '\n\a========== SL16 future-flow repeats complete ==========\n'
date '+Completed at %F %T %Z'
wall 'SL16 future-flow repeat campaign has completed.' 2>/dev/null || true
