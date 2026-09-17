#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/portable_greedy_heap/throttling_sweep"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local family=$1 team_size=$2 ratio=$3 label=$4 input output log
  case "$family" in
    random) input="$ROOT/instances/random/random_${team_size}.json" ;;
    warehouse-small) input="$ROOT/instances/warehouseSmall/warehouseSmall_${team_size}.json" ;;
    *) echo "Unsupported family: $family" >&2; return 2 ;;
  esac

  output="$OUT_DIR/${family}_${team_size}_cap${label}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -f "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -ge 0 ]]; then
    echo "Skipping completed output: $output"
    return 0
  fi

  "$BIN" \
    --inputFile "$input" \
    --output "$output" \
    --outputScreen 3 \
    --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 \
    --preprocessTimeLimit 30000 \
    --scheduleModel 6 \
    --useTraffic false \
    --assignNew false \
    --heapDistWeight 5 \
    --heapMaxAssign "$ratio" \
    --heapReassign true \
    --heapKeepBias 6 \
    --heapProtectDist 10 \
    --heapRebuildPct 45 \
    --heapLnsPct 10 \
    --heapSortK 500 \
    --logDetailLevel 2 >"$log" 2>&1
}

for spec in \
  'warehouse-small 600 0.4 40' \
  'warehouse-small 600 0.6 60' \
  'warehouse-small 600 0.8 80' \
  'warehouse-small 600 1.0 100' \
  'random 1200 0.4 40' \
  'random 1200 0.6 60' \
  'random 1200 0.8 80' \
  'random 1200 1.0 100' \
  'random 2000 0.4 40' \
  'random 2000 0.6 60' \
  'random 2000 0.8 80' \
  'random 2000 1.0 100'; do
  read -r family team_size ratio label <<<"$spec"
  run_one "$family" "$team_size" "$ratio" "$label"
done
