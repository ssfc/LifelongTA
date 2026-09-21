#!/usr/bin/env bash
set -euo pipefail

# Rebuild the released Table 2 baseline with actualMakespan: the last task
# completion time, rather than the legacy max-agent-active-time field.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="${OUT_ROOT:-$ROOT/results/table2_actual_rebaseline}"
SEED_COUNT="${1:-25}"
STEPS="${STEPS:-400}"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
[[ "$SEED_COUNT" =~ ^[1-9][0-9]*$ ]] || { echo "Seed count must be positive" >&2; exit 2; }

is_complete() {
  local output=$1
  [[ -s "$output" ]] || return 1
  [[ $(jq -r '.numTaskFinished // -1' "$output") -eq 500 ]] || return 1
  [[ $(jq -r '.allTasksCompleted // false' "$output") == true ]] || return 1
  [[ $(jq -r '.actualMakespan // empty' "$output") =~ ^[0-9]+$ ]]
}

run_one() {
  local method=$1 model=$2 assign_new=$3 f=$4 n=$5 seed=$6
  local stem="problem_${n}_${f}_${seed}"
  local dir="$OUT_ROOT/f${f}_n${n}"
  local output="$dir/${method}_${seed}.json"
  local log="$dir/${method}_${seed}.log"
  mkdir -p "$dir"

  if is_complete "$output"; then
    return
  fi

  printf '[%s] %s f=%s n=%s seed=%s\n' "$(date '+%F %T')" "$method" "$f" "$n" "$seed"
  "$BIN" \
    --inputFile "$ROOT/instances/${stem}.json" \
    --output "$output" \
    --simulationTime "$STEPS" \
    --planTimeLimit 1000 \
    --preprocessTimeLimit 30000 \
    --scheduleModel "$model" \
    --useTraffic 0 \
    --assignNew "$assign_new" \
    --commitWindow 1 \
    --outputScreen 3 \
    --logDetailLevel 3 \
    --heapDistWeight 5 \
    --heapReassign 1 \
    --heapKeepBias 6 \
    --heapProtectDist 10 \
    --heapRebuildPct 45 \
    --heapLnsPct 10 \
    --heapSortK 500 >"$log" 2>&1

  is_complete "$output" || {
    echo "Incomplete result: $output" >&2
    return 1
  }
}

for f in 2 5 10; do
  for n in 50 80 100; do
    for seed in $(seq 0 $((SEED_COUNT - 1))); do
      run_one greedy 5 1 "$f" "$n" "$seed"
      run_one flow_unit 1 0 "$f" "$n" "$seed"
      run_one greedy_heap 6 1 "$f" "$n" "$seed"
    done
  done
done

"$ROOT/scripts/summarize_table2_actual.py" "$OUT_ROOT" "$OUT_ROOT/actual_makespan_summary.csv"
printf '\n\a========== Corrected Table 2 baseline complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Corrected Table 2 baseline has completed.' 2>/dev/null || true
