#!/usr/bin/env bash
set -euo pipefail

# Screen bounded deterministic local matching repair on the local SL16 setup.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sl16_local_exchange_screen"
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 top_k=$2
  local output="$OUT_DIR/sortation-large_16000_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and .error == null' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s: local-exchange-top-k=%s\n' "$(date '+%F %T')" "$label" "$top_k"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_16000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --heapDistWeight 5 \
    --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 \
    --heapFlowZoneRows 20 --heapFlowZoneCols 10 --heapFlowPenaltyWeight 1.0 \
    --heapAgeBonus 0.0 --heapLocalExchangeTopK "$top_k" --logDetailLevel 2 >"$log" 2>&1
}

run_one exchange0 0
run_one exchange4 4
run_one exchange8 8
run_one exchange16 16

printf '\n\a========== SL16 local-exchange screen complete ==========\n'
date '+Completed at %F %T %Z'
wall 'SL16 local-exchange screen has completed.' 2>/dev/null || true
