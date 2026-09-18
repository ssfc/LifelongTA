#!/usr/bin/env bash
set -euo pipefail

# First screen for partial TaskMatcher.  Ratio 1.0 is the current full-match
# baseline; lower values commit only the lowest-cost part of each matching.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR="$ROOT/results/portable_assignment/w600_partial_screen"
TIMESTEPS=${1:-100}
INPUT="$ROOT/instances/warehouseSmall/warehouseSmall_600.json"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for ratio in 1.0 0.9 0.75 0.5; do
  tag=${ratio/./p}
  output="$OUT_DIR/w600_ratio${tag}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    continue
  fi
  echo "Running W600 partial TaskMatcher ratio=$ratio"
  "$BIN" --inputFile "$INPUT" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 2 --matcherDistWeight 10 --matcherTopK 100 \
    --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 --matcherMaxAssign "$ratio" >"$log" 2>&1
done

printf '\n\a========== W600 partial TaskMatcher screen complete ==========\n'
date '+Completed at %F %T %Z'
