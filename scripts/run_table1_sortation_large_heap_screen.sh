#!/usr/bin/env bash
set -euo pipefail

# Short large-scale comparison for the established PortableGreedyHeap baseline.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/table1_sortation_large_heap_screen"}
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for agents in 8000 12000; do
  input="$ROOT/instances/sortationLarge/sortationLarge_${agents}.json"
  output="$OUT_DIR/sortation-large_${agents}_heap-w10-k1000_t${TIMESTEPS}.json"
  log="${output%.json}.log"

  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    continue
  fi

  echo "Running PortableGreedyHeap on Sortation Large n=$agents"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 6 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 2 --heapDistWeight 10 --heapMaxAssign 1.0 \
    --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapSortK 1000 --heapRebuildPct 45 --heapLnsPct 10 >"$log" 2>&1
done
