#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/portable_greedy_heap/lns20_dev"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local family=$1 team_size=$2 input output log
  case "$family" in
    random) input="$ROOT/instances/random/random_${team_size}.json" ;;
    warehouse-small) input="$ROOT/instances/warehouseSmall/warehouseSmall_${team_size}.json" ;;
    *) echo "Unsupported family: $family" >&2; return 2 ;;
  esac
  output="$OUT_DIR/${family}_${team_size}_w10_lns20_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -f "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    return 0
  fi

  "$BIN" \
    --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 6 --useTraffic false --assignNew false \
    --heapDistWeight 10 --heapMaxAssign 1.0 \
    --heapReassign true --heapKeepBias 6 --heapProtectDist 10 \
    --heapRebuildPct 45 --heapLnsPct 20 --heapSortK 500 \
    --logDetailLevel 2 >"$log" 2>&1
}

for spec in 'warehouse-small 600' 'random 1200' 'random 2000'; do
  read -r family team_size <<<"$spec"
  run_one "$family" "$team_size"
done
