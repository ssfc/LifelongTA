#!/usr/bin/env bash
set -euo pipefail

# Full-horizon validation of the only positive SL16 waiting-age setting.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sl16_age_bonus_followup"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 age_bonus=$2
  local output="$OUT_DIR/sortation-large_16000_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and .error == null' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s: age-bonus=%s\n' "$(date '+%F %T')" "$label" "$age_bonus"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_16000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --heapDistWeight 5 \
    --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 \
    --heapFlowZoneRows 20 --heapFlowZoneCols 10 --heapFlowPenaltyWeight 1.0 \
    --heapAgeBonus "$age_bonus" --logDetailLevel 2 >"$log" 2>&1
}

run_one age0 0.0
run_one age050 0.5

printf '\n\a========== SL16 age-bonus follow-up complete ==========\n'
date '+Completed at %F %T %Z'
wall 'SL16 age-bonus full follow-up has completed.' 2>/dev/null || true
