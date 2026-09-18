#!/usr/bin/env bash
set -euo pipefail

# Long-horizon confirmation of the short-screen winners on Table 1 loss cells.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/portable_assignment/table1_loss_confirm"
TIMESTEPS=${1:-1000}

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
  run_one free_reassign "$family" "$agents" \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherReassign true --heapKeepBias 0 --heapProtectDist 0
done

# The short screen differed by one task at Random 2,000; retain this near tie.
run_one stable_reassign 'random' 2000 \
  --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
  --matcherReassign true --heapKeepBias 12 --heapProtectDist 20

printf '\n\a========== TaskMatcher Table 1 loss confirmation complete ==========\n'
date '+Completed at %F %T %Z'
wall 'TaskMatcher Table 1 long-horizon loss confirmation has completed.' 2>/dev/null || true
if command -v notify-send >/dev/null 2>&1; then
  notify-send --urgency=normal 'TaskMatcher experiment complete' \
    'The Table 1 long-horizon loss confirmation has finished.' || true
fi
