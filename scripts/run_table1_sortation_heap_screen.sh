#!/usr/bin/env bash
set -euo pipefail

# Targeted strict-budget screen for the two Sortation Large rows where the
# frozen GreedyHeap configuration trails the best released Flow variant.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sortation_heap_screen"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local agents=$1 label=$2
  shift 2
  local input="$ROOT/instances/sortationLarge/sortationLarge_${agents}.json"
  local output="$OUT_DIR/sortation-large_${agents}_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"

  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and has("numPlannerErrors") and has("numScheduleErrors")' "$output" >/dev/null; then
    return
  fi

  printf '[%s] %s sortation-large n=%s\n' "$(date '+%F %T')" "$label" "$agents"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 6 --useTraffic false --assignNew false --commitWindow 1 \
    --heapDistWeight 5 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapSortK 500 --logDetailLevel 2 "$@" >"$log" 2>&1

  [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and has("numPlannerErrors") and has("numScheduleErrors")' "$output" >/dev/null || {
    echo "Missing or malformed result: $output" >&2
    return 1
  }
}

for agents in 16000 20000; do
  run_one "$agents" throttle80 --heapMaxAssign 0.8 --heapLnsPct 10
  run_one "$agents" throttle60 --heapMaxAssign 0.6 --heapLnsPct 10
  run_one "$agents" no_lns --heapMaxAssign 1.0 --heapLnsPct 0
done

printf '\n\a========== Sortation Large GreedyHeap screen complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Sortation Large GreedyHeap screening has completed.' 2>/dev/null || true
