#!/usr/bin/env bash
set -euo pipefail

# Targeted short-horizon screen for the Table 1 cells where TaskMatcher trails Flow.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/portable_assignment/table1_loss_screen"
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 family=$2 agents=$3
  shift 3
  local input output log
  case "$family" in
    warehouse-small) input="$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json" ;;
    random) input="$ROOT/instances/random/random_${agents}.json" ;;
    *) echo "Unsupported family: $family" >&2; return 2 ;;
  esac
  output="$OUT_DIR/${family}_${agents}_${label}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    return
  fi
  echo "Running $label on $family n=$agents for $TIMESTEPS timesteps"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 2 "$@" >"$log" 2>&1
}

for spec in 'warehouse-small 500' 'warehouse-small 600' 'random 400' \
            'random 800' 'random 1200' 'random 2000'; do
  read -r family agents <<<"$spec"
  run_one baseline_dw10 "$family" "$agents" \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 6 --heapProtectDist 10
  run_one dw5 "$family" "$agents" \
    --matcherDistWeight 5 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 6 --heapProtectDist 10
  run_one dw15 "$family" "$agents" \
    --matcherDistWeight 15 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 6 --heapProtectDist 10
  run_one no_reassign "$family" "$agents" \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign false --heapKeepBias 6 --heapProtectDist 10
  run_one free_reassign "$family" "$agents" \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 0 --heapProtectDist 0
  run_one stable_reassign "$family" "$agents" \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 12 --heapProtectDist 20
done

printf '\n\a========== TaskMatcher Table 1 loss screen complete ==========\n'
date '+Completed at %F %T %Z'
wall 'TaskMatcher Table 1 loss-focused screening has completed.' 2>/dev/null || true
if command -v notify-send >/dev/null 2>&1; then
  notify-send --urgency=normal 'TaskMatcher experiment complete' \
    'The Table 1 loss-focused screening has finished.' || true
fi
