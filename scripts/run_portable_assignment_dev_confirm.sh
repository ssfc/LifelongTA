#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/portable_assignment/dev_confirm"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local model=$1 label=$2 family=$3 agents=$4
  shift 4
  local input output log
  case "$family" in
    warehouse-small) input="$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json" ;;
    random) input="$ROOT/instances/random/random_${agents}.json" ;;
    *) echo "Unsupported family: $family" >&2; return 2 ;;
  esac
  output="$OUT_DIR/${family}_${agents}_${label}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed output: $output"
    return
  fi
  echo "Running $label on $family n=$agents for $TIMESTEPS timesteps"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel "$model" --useTraffic false --assignNew false --logDetailLevel 2 "$@" >"$log" 2>&1
}

for spec in 'warehouse-small 200' 'warehouse-small 400' 'random 400' 'random 800'; do
  read -r family agents <<<"$spec"
  run_one 7 taskmatcher_dw10_k100 "$family" "$agents" \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 --matcherReassign true
  run_one 8 cappedhungarian_dw5_lw0.5 "$family" "$agents" \
    --hungarianMaxAgents 256 --hungarianMaxTasks 512 \
    --hungarianDistWeight 5 --hungarianTaskLengthWeight 0.5
done
