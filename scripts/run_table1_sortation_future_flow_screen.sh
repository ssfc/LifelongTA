#!/usr/bin/env bash
set -euo pipefail

# Scheduler-only future-flow screen. Every row keeps the fixed LifelongTA
# planner/executor and changes only PortableGreedyHeap's candidate score.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sortation_future_flow_screen"
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local agents=$1 label=$2 ratio=$3 weight=$4
  local input="$ROOT/instances/sortationLarge/sortationLarge_${agents}.json"
  local output="$OUT_DIR/sortation-large_${agents}_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and .error == null' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s: Sortation Large n=%s, ratio=%s, flow-weight=%s\n' \
    "$(date '+%F %T')" "$label" "$agents" "$ratio" "$weight"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 6 --useTraffic false --assignNew false --commitWindow 1 \
    --heapDistWeight 5 --heapMaxAssign "$ratio" --heapReassign true \
    --heapKeepBias 6 --heapProtectDist 10 --heapRebuildPct 45 --heapLnsPct 10 \
    --heapSortK 500 --heapFlowZoneRows 20 --heapFlowZoneCols 10 \
    --heapFlowPenaltyWeight "$weight" --logDetailLevel 2 >"$log" 2>&1
}

for spec in '16000 full 1.0' '20000 throttle60 0.6'; do
  read -r agents label ratio <<<"$spec"
  run_one "$agents" "${label}_baseline" "$ratio" 0.0
  run_one "$agents" "${label}_flow025" "$ratio" 0.25
  run_one "$agents" "${label}_flow100" "$ratio" 1.00
done

printf '\n\a========== Sortation Large future-flow screen complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Sortation Large future-flow screen has completed.' 2>/dev/null || true
