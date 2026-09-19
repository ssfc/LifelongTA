#!/usr/bin/env bash
set -euo pipefail

# Table 2 low-density cost-model screen. All variants retain the released
# workload, the 400-step cap, and the one-second planner budget.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_taskmatcher_cost_screen"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local tag=$1 pickup_weight=$2 length_weight=$3 f=$4 n=$5 seed=$6 output log
  output="$OUT_ROOT/$tag/f${f}_n${n}/taskmatcher_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then
    return
  fi
  echo "[$(date '+%F %T %Z')] $tag f=$f n=$n seed=$seed"
  "$BIN" --inputFile "$ROOT/instances/problem_${n}_${f}_${seed}.json" \
    --output "$output" --outputScreen 3 --simulationTime 400 \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 7 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
    --matcherDistWeight "$pickup_weight" --matcherTaskLengthWeight "$length_weight" \
    --matcherTopK 100 --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 >"$log" 2>&1
}

for spec in 'p1_l1 1 1' 'p2_l1 2 1' 'p1_l2 1 2' 'p1_l0.5 1 0.5'; do
  read -r tag pickup_weight length_weight <<<"$spec"
  for cell in '2 80' '2 100' '5 50' '10 50' '10 100'; do
    read -r f n <<<"$cell"
    for seed in $(seq 0 4); do
      run_one "$tag" "$pickup_weight" "$length_weight" "$f" "$n" "$seed"
    done
  done
done

printf '\aTable 2 TaskMatcher cost-model screen complete\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 TaskMatcher cost-model screen has completed.\n' | wall 2>/dev/null || true
