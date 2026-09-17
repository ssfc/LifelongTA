#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/portable_greedy_heap/random_w15_k1000_dev"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for team_size in 1200 2000; do
  output="$OUT_DIR/random_${team_size}_w15_k1000_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -f "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    continue
  fi
  "$BIN" \
    --inputFile "$ROOT/instances/random/random_${team_size}.json" \
    --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 6 --useTraffic false --assignNew false \
    --heapDistWeight 15 --heapMaxAssign 1.0 \
    --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 10 --heapSortK 1000 \
    --logDetailLevel 2 >"$log" 2>&1
done
