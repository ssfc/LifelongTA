#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TIMESTEPS=${1:-250}
FAIR_K=${2:-8}
REPLICATE=${3:-1}
OUT_DIR="$ROOT/results/table1_sl16_exact_shortlist"
mkdir -p "$OUT_DIR"

suffix=""
if [[ "$REPLICATE" != 1 ]]; then suffix="_rep${REPLICATE}"; fi
output="$OUT_DIR/sl16_exact_k${FAIR_K}_t${TIMESTEPS}${suffix}.json"
log="${output%.json}.log"
printf '[%s] SL16 exact-pickup shortlist, %s timesteps, replicate %s\n' \
  "$(date '+%F %T')" "$TIMESTEPS" "$REPLICATE"
"$ROOT/build/lifelong" \
  --inputFile "$ROOT/instances/sortationLarge/sortationLarge_16000.json" \
  --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
  --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
  --useTraffic false --assignNew false --commitWindow 1 --heapDistWeight 5 \
  --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
  --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 \
  --heapFlowZoneRows 20 --heapFlowZoneCols 10 --heapFlowPenaltyWeight 1.0 \
  --heapFairCandidateK "$FAIR_K" --heapExactPickupCache true --heapCandidateDiagEvery 25 \
  --logDetailLevel 2 >"$log" 2>&1
printf '\n========== SL16 exact shortlist complete ==========\n'
jq '{numTaskFinished, error}' "$output"
