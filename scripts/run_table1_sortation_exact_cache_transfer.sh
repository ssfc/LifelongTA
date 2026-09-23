#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
AGENTS=${1:?usage: script AGENTS TIMESTEPS [REPLICATE]}
TIMESTEPS=${2:?usage: script AGENTS TIMESTEPS [REPLICATE]}
REPLICATE=${3:-1}
OUT_DIR="$ROOT/results/table1_sortation_exact_cache_transfer"
mkdir -p "$OUT_DIR"

case "$AGENTS" in
  4000|8000|12000) ratio=1.0; flow_weight=0.0 ;;
  20000) ratio=0.6; flow_weight=1.0 ;;
  *) echo "Unsupported agent count: $AGENTS" >&2; exit 2 ;;
esac

suffix=""
if [[ "$REPLICATE" != 1 ]]; then suffix="_rep${REPLICATE}"; fi
output="$OUT_DIR/sortation-large_${AGENTS}_exact_cache_t${TIMESTEPS}${suffix}.json"
log="${output%.json}.log"
if [[ -s "$output" ]] && jq -e '.error == null and (.numTaskFinished | type) == "number"' "$output" >/dev/null; then
  printf 'Already complete: %s\n' "$output"
  exit 0
fi

printf '[%s] SL%s exact cache, %s timesteps, replicate %s\n' \
  "$(date '+%F %T')" "$AGENTS" "$TIMESTEPS" "$REPLICATE"
"$ROOT/build/lifelong" \
  --inputFile "$ROOT/instances/sortationLarge/sortationLarge_${AGENTS}.json" \
  --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
  --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
  --useTraffic false --assignNew false --commitWindow 1 --heapDistWeight 5 \
  --heapMaxAssign "$ratio" --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
  --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 \
  --heapFlowZoneRows 20 --heapFlowZoneCols 10 --heapFlowPenaltyWeight "$flow_weight" \
  --heapExactPickupCache true --heapCandidateDiagEvery 25 --logDetailLevel 2 >"$log" 2>&1
jq '{numTaskFinished, numEntryTimeouts, numPlannerErrors, numScheduleErrors, error}' "$output"
