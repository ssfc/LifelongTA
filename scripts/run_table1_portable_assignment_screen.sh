#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/portable_assignment/table1_screen"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local model=$1 name=$2 family=$3 agents=$4 input output log
  case "$family" in
    warehouse-small) input="$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json" ;;
    random) input="$ROOT/instances/random/random_${agents}.json" ;;
    *) echo "Unsupported family: $family" >&2; return 2 ;;
  esac
  output="$OUT_DIR/${family}_${agents}_${name}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    return
  fi
  echo "Running $name on $family n=$agents for $TIMESTEPS timesteps"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel "$model" --useTraffic false --assignNew false --logDetailLevel 2 >"$log" 2>&1
}

# CappedHungarian has a 256-agent candidate cap. These points establish the
# small/medium-scale baseline before any larger, timeout-prone sweep.
for spec in 'warehouse-small 200' 'random 400'; do
  read -r family agents <<<"$spec"
  run_one 7 taskmatcher "$family" "$agents"
  run_one 8 capped_hungarian "$family" "$agents"
done
