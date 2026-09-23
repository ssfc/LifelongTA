#!/usr/bin/env bash
set -euo pipefail

# Local, same-machine comparison for SL16. Both methods use the released input,
# DefaultPlanner execution, commitWindow=1, and a 1 s per-timestep budget.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sl16_local_flow_paired"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local method=$1 rep=$2
  local output="$OUT_DIR/sortation-large_16000_${method}_rep${rep}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and .error == null' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s rep=%s\n' "$(date '+%F %T')" "$method" "$rep"
  local args=(
    --inputFile "$ROOT/instances/sortationLarge/sortationLarge_16000.json"
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS"
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --assignNew false --commitWindow 1
    --logDetailLevel 2
  )
  if [[ "$method" == flow_traffic ]]; then
    args+=(--scheduleModel 1 --useTraffic true)
  else
    args+=(
      --scheduleModel 6 --useTraffic false --heapDistWeight 5 --heapMaxAssign 1.0
      --heapReassign true --heapKeepBias 6 --heapProtectDist 10 --heapRebuildPct 45
      --heapLnsPct 10 --heapSortK 500 --heapFlowZoneRows 20 --heapFlowZoneCols 10
      --heapFlowPenaltyWeight 1.0
    )
  fi
  "$BIN" "${args[@]}" >"$log" 2>&1
}

for rep in 1 2 3; do
  run_one flow_traffic "$rep"
  run_one greedy_heap_future_flow "$rep"
done

printf '\n\a========== SL16 local paired Flow comparison complete ==========\n'
date '+Completed at %F %T %Z'
wall 'SL16 local paired Flow comparison has completed.' 2>/dev/null || true
