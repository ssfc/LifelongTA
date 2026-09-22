#!/usr/bin/env bash
set -euo pipefail

# Targeted screen for the SL-12000 GreedyHeap gap to Flow-Avg Waiting.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/sl12000_heap_screen"}
TIMESTEPS=${1:-100}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

# name, pickup-distance weight, keep bias, pickup protection, rebuild %, LNS %, retained candidates
configs=(
  'baseline-w10-b6-p10-r45-l10-k1000 10 6 10 45 10 1000'
  'pickup-w15-b6-p10-r45-l10-k1000 15 6 10 45 10 1000'
  'route-w5-b6-p10-r45-l10-k1000 5 6 10 45 10 1000'
  'rebuild-w10-b6-p10-r70-l0-k1000 10 6 10 70 0 1000'
  'stable-w10-b20-p20-r45-l10-k1000 10 20 20 45 10 1000'
  'compact-w10-b6-p10-r70-l0-k250 10 6 10 70 0 250'
)

for config in "${configs[@]}"; do
  read -r name weight bias protect rebuild lns sort_k <<<"$config"
  output="$OUT_DIR/sl12000_${name}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed $name"
    continue
  fi

  echo "Running SL12000 GreedyHeap $name"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_12000.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 6 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --heapDistWeight "$weight" --heapMaxAssign 1.0 --heapReassign true \
    --heapKeepBias "$bias" --heapProtectDist "$protect" \
    --heapSortK "$sort_k" --heapRebuildPct "$rebuild" --heapLnsPct "$lns" >"$log" 2>&1
done
