#!/usr/bin/env bash
set -euo pipefail

# Five paired public task-stream seeds per Table 2 cell. This preserves the
# finite-workload protocol and only replaces the assignment scheduler.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
INPUT_ROOT=${INPUT_ROOT:-"/c/gitcloud/flow-compare"}
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR="$ROOT/results/traffic_taskmatcher_table2_screen"
TIMESTEPS=${1:-400}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
[[ -d "$INPUT_ROOT/instances" ]] || { echo "Missing Table 2 inputs: $INPUT_ROOT/instances" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local f=$1 n=$2 seed=$3 input output log
  input="$INPUT_ROOT/instances/problem_${n}_${f}_${seed}.json"
  output="$OUT_DIR/f${f}_n${n}/taskmatcher_traffic_k100_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  [[ -f "$input" ]] || { echo "Missing input: $input" >&2; return 2; }
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then
    return
  fi
  echo "Running TrafficAware TaskMatcher f=$f n=$n seed=$seed"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --finiteTaskStream true --commitWindow 1 \
    --logDetailLevel 2 --matcherDistWeight 10 --matcherTopK 100 \
    --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 --matcherUseTraffic true \
    --matcherTrafficTopK 100 --matcherTrafficCongestionWeight 1.0 >"$log" 2>&1
}

for f in 2 5 10; do
  for n in 50 80 100; do
    for seed in 0 1 2 3 4; do
      run_one "$f" "$n" "$seed"
    done
  done
done

printf '\n\a========== TrafficAware TaskMatcher Table 2 screen complete ==========\n'
date '+Completed at %F %T %Z'
