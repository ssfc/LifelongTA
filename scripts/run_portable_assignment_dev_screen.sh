#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/portable_assignment/dev_screen"
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local label=$1 family=$2 agents=$3
  shift 3
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
    --useTraffic false --assignNew false --logDetailLevel 2 "$@" >"$log" 2>&1
}

# Development-only screen. The defaults inherited from the contest source are
# reassign=true, keep bias=6, and pickup-distance protection=10.
for spec in 'warehouse-small 200' 'warehouse-small 400' 'random 400' 'random 800'; do
  read -r family agents <<<"$spec"
  for weight in 2 5 10; do
    for topk in 25 50 100; do
      run_one "taskmatcher_dw${weight}_k${topk}" "$family" "$agents" \
        --scheduleModel 7 --matcherDistWeight "$weight" --matcherTopK "$topk" \
        --matcherMaxMatrix 2000000 --matcherReassign true
    done
  done
  for distance_weight in 1 5; do
    for length_weight in 0.5 1; do
      run_one "cappedhungarian_dw${distance_weight}_lw${length_weight}" "$family" "$agents" \
        --scheduleModel 8 --hungarianMaxAgents 256 --hungarianMaxTasks 512 \
        --hungarianDistWeight "$distance_weight" --hungarianTaskLengthWeight "$length_weight"
    done
  done
done
