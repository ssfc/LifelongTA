#!/usr/bin/env bash
set -euo pipefail

# Confirm the spatial-candidate TaskMatcher on the two densities that exceeded
# the former all-task-scan fallback budget.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR="$ROOT/results/portable_assignment/table1_sortation_large_spatial_confirm"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for agents in 16000 20000; do
  input="$ROOT/instances/sortationLarge/sortationLarge_${agents}.json"
  output="$OUT_DIR/sortation-large_${agents}_spatial_t${TIMESTEPS}.json"
  log="${output%.json}.log"

  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    continue
  fi

  echo "Running spatial-candidate TaskMatcher on Sortation Large n=$agents"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 2 --matcherDistWeight 10 --matcherTopK 100 \
    --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 >"$log" 2>&1
done
