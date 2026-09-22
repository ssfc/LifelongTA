#!/usr/bin/env bash
set -euo pipefail

# Compare the scheduler-only mechanisms that remain relevant on the two
# Sortation Large rows where GreedyHeap trails Flow-Traffic. The result fields
# added for this campaign are observational timing summaries only.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sortation_diagnostic"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local agents=$1 label=$2 schedule_model=$3 use_traffic=$4 max_assign=$5
  local input="$ROOT/instances/sortationLarge/sortationLarge_${agents}.json"
  local output="$OUT_DIR/sortation-large_${agents}_${label}_reference_t${TIMESTEPS}.json"
  local log="${output%.json}.log"

  if [[ -s "$output" ]] && jq -e 'has("avgRevealToAssign") and has("avgAssignToOpen") and has("avgOpenToFinish")' "$output" >/dev/null; then
    return
  fi

  printf '[%s] %s sortation-large n=%s\n' "$(date '+%F %T')" "$label" "$agents"
  local base_args=(
    --inputFile "$input" --output "$output" --outputScreen 3
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000
    --scheduleModel "$schedule_model" --useTraffic "$use_traffic" --assignNew false
    --logDetailLevel 2
  )
  if [[ "$schedule_model" == 6 ]]; then
    base_args+=(
      --commitWindow 1 --heapDistWeight 5 --heapMaxAssign "$max_assign"
      --heapReassign true --heapKeepBias 6 --heapProtectDist 10 --heapRebuildPct 45
      --heapLnsPct 10 --heapSortK 500
    )
  fi
  "$BIN" "${base_args[@]}" >"$log" 2>&1

  jq -e 'has("avgRevealToAssign") and has("avgAssignToOpen") and has("avgOpenToFinish")' "$output" >/dev/null || {
    echo "Missing diagnostic fields: $output" >&2
    return 1
  }
}

run_one 16000 flow_traffic 1 true 1.0
run_one 16000 greedy_heap 6 false 1.0
run_one 20000 flow_traffic 1 true 1.0
run_one 20000 greedy_heap 6 false 1.0
run_one 20000 greedy_heap_throttle60 6 false 0.6

printf '\n\a========== Sortation Large diagnostic campaign complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Sortation Large diagnostic campaign has completed.' 2>/dev/null || true
