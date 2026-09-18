#!/usr/bin/env bash
set -euo pipefail

# Complete Table 1 under the one free-reassignment TaskMatcher configuration.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/portable_assignment/table1_free_completion"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local family=$1 agents=$2 input output log
  case "$family" in
    warehouse-small) input="$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json" ;;
    random) input="$ROOT/instances/random/random_${agents}.json" ;;
    *) echo "Unsupported family: $family" >&2; return 2 ;;
  esac
  output="$OUT_DIR/${family}_${agents}_free_reassign_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    return
  fi
  echo "Running free_reassign on $family n=$agents for $TIMESTEPS timesteps"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 2 --matcherDistWeight 10 --matcherTopK 100 \
    --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 >"$log" 2>&1
}

for spec in 'warehouse-small 200' 'warehouse-small 300' 'warehouse-small 400' 'random 1600'; do
  read -r family agents <<<"$spec"
  run_one "$family" "$agents"
done

printf '\n\a========== TaskMatcher Table 1 free-reassignment completion complete ==========\n'
date '+Completed at %F %T %Z'
wall 'TaskMatcher Table 1 free-reassignment completion has completed.' 2>/dev/null || true
