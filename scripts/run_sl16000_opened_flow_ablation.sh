#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build-ucrt/lifelong.exe"
OUT_DIR="$ROOT/results/portable_assignment/sl16000_opened_flow_ablation"
TIMESTEPS=${1:-100}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1; shift
  local output="$OUT_DIR/sl16000_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and .error == null' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s\n' "$(date '+%F %T')" "$label"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_16000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 "$@" >"$log" 2>&1
}

# Direct protocol reproduction of main's successful coarse opened-task flow pair.
run_one main_static \
  --heapDistWeight 5 --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6 \
  --heapProtectDist 10 --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500
run_one main_opened_flow \
  --heapDistWeight 5 --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6 \
  --heapProtectDist 10 --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 \
  --heapOpenedFlowZoneRows 20 --heapOpenedFlowZoneCols 10 --heapOpenedFlowPenaltyWeight 1.0

# Current scalable dynamic policy, before and after adding the independent opened-task signal.
common_dynamic=(
  --heapDistWeight 10 --heapMaxAssign 1.0 --heapReassign true --heapKeepBias 6
  --heapProtectDist 10 --heapSortK 250 --heapRebuildPct 70 --heapLnsPct 10
  --heapFlowAwareSpatial true --heapSpatialCell 16 --heapSpatialCandidates 1000
  --heapTrafficWeight 0.25 --heapReassignMinGain 10
  --heapFutureFlowCell 32 --heapFutureFlowWeight 0.01 --heapFutureFlowHardCap 0
  --heapFutureFlowRegret false
)
run_one dynamic "${common_dynamic[@]}"
run_one dynamic_opened_flow "${common_dynamic[@]}" \
  --heapOpenedFlowZoneRows 20 --heapOpenedFlowZoneCols 10 --heapOpenedFlowPenaltyWeight 1.0

printf '\aSL16000 opened-flow ablation complete\n'
