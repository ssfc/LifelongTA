#!/usr/bin/env bash
set -euo pipefail

# Follow up the only useful large-sortation screen signal: 60% assignment
# throttling at n=20,000. Each run changes one factor from that configuration.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sortation_n20_heap_followup"
TIMESTEPS=${1:-1000}
INPUT="$ROOT/instances/sortationLarge/sortationLarge_20000.json"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 ratio=$2 dist_weight=$3
  local output="$OUT_DIR/sortation-large_20000_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and has("numPlannerErrors") and has("numScheduleErrors")' "$output" >/dev/null; then
    return
  fi

  printf '[%s] %s sortation-large n=20000\n' "$(date '+%F %T')" "$label"
  "$BIN" --inputFile "$INPUT" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 6 --useTraffic false --assignNew false --commitWindow 1 \
    --heapDistWeight "$dist_weight" --heapMaxAssign "$ratio" \
    --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 --logDetailLevel 2 >"$log" 2>&1

  [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and has("numPlannerErrors") and has("numScheduleErrors")' "$output" >/dev/null || {
    echo "Missing or malformed result: $output" >&2
    return 1
  }
}

run_one throttle50 0.5 5
run_one throttle70 0.7 5
run_one throttle60_dist10 0.6 10

printf '\n\a========== Sortation Large n=20k follow-up complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Sortation Large 20k GreedyHeap follow-up is complete.' 2>/dev/null || true
