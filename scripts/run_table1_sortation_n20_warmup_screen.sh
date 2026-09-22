#!/usr/bin/env bash
set -euo pipefail

# Screen whether a brief full-assignment phase reduces the queueing introduced
# by the 60% GreedyHeap cap before congestion becomes dominant.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sortation_n20_warmup_screen"
TIMESTEPS=${1:-250}
INPUT="$ROOT/instances/sortationLarge/sortationLarge_20000.json"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 warmup_steps=$2
  local output="$OUT_DIR/sortation-large_20000_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished")' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s sortation-large n=20000\n' "$(date '+%F %T')" "$label"
  "$BIN" --inputFile "$INPUT" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 6 --useTraffic false --assignNew false --commitWindow 1 \
    --heapDistWeight 5 --heapMaxAssign 0.6 --heapWarmupSteps "$warmup_steps" \
    --heapWarmupAssign 1.0 --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 500 --logDetailLevel 2 >"$log" 2>&1
}

run_one throttle60 0
run_one warmup25 25
run_one warmup75 75
run_one warmup150 150

printf '\n\a========== Sortation Large 20k warm-up screen complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Sortation Large 20k warm-up screen has completed.' 2>/dev/null || true
