#!/usr/bin/env bash
set -euo pipefail

# Complete the five released Sortation Large Table 1 rows for the two portable
# schedulers used in the paper. Released baselines are already present.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_ours_sortation_completion"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local method=$1 agents=$2 input output log
  input="$ROOT/instances/sortationLarge/sortationLarge_${agents}.json"
  output="$OUT_DIR/sortation-large_${agents}_${method}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  [[ -f "$input" ]] || { echo "Missing instance: $input" >&2; return 1; }

  # Zero completed tasks is a valid strict-budget outcome on the largest
  # instances. Only a malformed or missing JSON output should be retried.
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and has("numPlannerErrors") and has("numScheduleErrors")' "$output" >/dev/null; then
    return
  fi

  printf '[%s] %s sortation-large n=%s\n' "$(date '+%F %T')" "$method" "$agents"
  case "$method" in
    greedy_heap)
      "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
        --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
        --scheduleModel 6 --useTraffic false --assignNew false --commitWindow 1 \
        --heapDistWeight 5 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
        --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 --logDetailLevel 2 >"$log" 2>&1
      ;;
    taskmatcher_free)
      "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
        --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
        --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
        --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
        --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 --logDetailLevel 2 >"$log" 2>&1
      ;;
    *) echo "Unknown method: $method" >&2; return 2 ;;
  esac

  [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and has("numPlannerErrors") and has("numScheduleErrors")' "$output" >/dev/null || {
    echo "Missing or empty result: $output" >&2
    return 1
  }
}

for agents in 4000 8000 12000 16000 20000; do
  run_one greedy_heap "$agents"
  run_one taskmatcher_free "$agents"
done

printf '\n\a========== Sortation Large Table 1 completion complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Sortation Large Table 1 completion has completed.' 2>/dev/null || true
