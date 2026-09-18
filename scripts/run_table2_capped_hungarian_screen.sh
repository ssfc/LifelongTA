#!/usr/bin/env bash
set -euo pipefail

# Five-seed Table 2 screen for the contest mode-14 exact Hungarian matcher.
# At n <= 100 the candidate limits exceed the instance size, so this is full
# bipartite matching under the unchanged LifelongTA task and planner interface.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_capped_hungarian_screen"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local f=$1 n=$2 seed=$3 output log input
  input="$ROOT/instances/problem_${n}_${f}_${seed}.json"
  output="$OUT_ROOT/f${f}_n${n}/hungarian_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then
    return
  fi
  echo "Running exact Hungarian f=$f n=$n seed=$seed"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 8 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 3 --hungarianMaxAgents 256 --hungarianMaxTasks 512 \
    --hungarianDistWeight 1 --hungarianTaskLengthWeight 1 >"$log" 2>&1
}

for f in 2 5 10; do
  for n in 50 80 100; do
    for seed in $(seq 0 4); do
      run_one "$f" "$n" "$seed"
    done
  done
done

printf '\n\a========== Table 2 exact-Hungarian screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 exact-Hungarian screen has completed.\n' | wall 2>/dev/null || true
