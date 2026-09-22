#!/usr/bin/env bash
set -euo pipefail

# Validate the task-internal-length cache on the two Sortation Large rows
# where GreedyHeap trails Flow-Traffic. Parameters match the prior strict
# GreedyHeap configurations; only the scheduler implementation is updated.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sortation_cached_followup"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local agents=$1 label=$2 ratio=$3
  local input="$ROOT/instances/sortationLarge/sortationLarge_${agents}.json"
  local output="$OUT_DIR/sortation-large_${agents}_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and has("avgRevealToAssign")' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s sortation-large n=%s\n' "$(date '+%F %T')" "$label" "$agents"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 6 --useTraffic false --assignNew false --commitWindow 1 \
    --heapDistWeight 5 --heapMaxAssign "$ratio" --heapReassign true \
    --heapKeepBias 6 --heapProtectDist 10 --heapRebuildPct 45 --heapLnsPct 10 \
    --heapSortK 500 --logDetailLevel 2 >"$log" 2>&1
}

run_one 16000 cached_full 1.0
run_one 20000 cached_throttle60 0.6

printf '\n\a========== Sortation Large cached Heap follow-up complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Sortation Large cached GreedyHeap follow-up has completed.' 2>/dev/null || true
