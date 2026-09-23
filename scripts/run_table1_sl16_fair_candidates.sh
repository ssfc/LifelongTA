#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
TIMESTEPS=${1:-250}
OUT_DIR="$ROOT/results/table1_sl16_fair_candidates"
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 fair_k=$2
  local output="$OUT_DIR/sl16_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e '.error == null and (.numTaskFinished | type) == "number"' "$output" >/dev/null; then
    return
  fi
  printf '[%s] SL16 %s, %s timesteps\n' "$(date '+%F %T')" "$label" "$TIMESTEPS"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_16000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --heapDistWeight 5 \
    --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 \
    --heapFlowZoneRows 20 --heapFlowZoneCols 10 --heapFlowPenaltyWeight 1.0 \
    --heapFairCandidateK "$fair_k" --heapCandidateDiagEvery 25 \
    --logDetailLevel 2 >"$log" 2>&1
}

run_one baseline 0
run_one fair_k8 8
run_one fair_hybrid_k8 8

printf '\n\a========== SL16 fair-candidate comparison complete ==========\n'
date '+Completed at %F %T %Z'
wall 'SL16 fair-candidate comparison has completed.' 2>/dev/null || true
