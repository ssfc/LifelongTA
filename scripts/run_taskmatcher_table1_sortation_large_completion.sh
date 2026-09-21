#!/usr/bin/env bash
set -euo pipefail

# Complete the five Sortation Large rows of Table 1 with the fixed
# free-reassignment TaskMatcher configuration used for the other ten rows.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR="$ROOT/results/portable_assignment/table1_sortation_large_completion"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for agents in 4000 8000 12000 16000 20000; do
  input="$ROOT/instances/sortationLarge/sortationLarge_${agents}.json"
  output="$OUT_DIR/sortation-large_${agents}_free_reassign_t${TIMESTEPS}.json"
  log="${output%.json}.log"

  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    continue
  fi

  echo "Running free-reassignment TaskMatcher on Sortation Large n=$agents"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 2 --matcherDistWeight 10 --matcherTopK 100 \
    --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 >"$log" 2>&1
done
