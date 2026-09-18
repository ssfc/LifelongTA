#!/usr/bin/env bash
set -euo pipefail

# Expand the sole competitive five-seed CappedHungarian Table 2 cell.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_capped_hungarian_screen"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

for seed in $(seq 5 24); do
  input="$ROOT/instances/problem_100_2_${seed}.json"
  output="$OUT_ROOT/f2_n100/hungarian_${seed}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then
    continue
  fi
  echo "Running exact Hungarian f=2 n=100 seed=$seed"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 8 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 3 --hungarianMaxAgents 256 --hungarianMaxTasks 512 \
    --hungarianDistWeight 1 --hungarianTaskLengthWeight 1 >"$log" 2>&1
done

printf '\n\a========== Table 2 CappedHungarian f=2, n=100 expansion complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 CappedHungarian f=2, n=100 expansion has completed.\n' | wall 2>/dev/null || true
