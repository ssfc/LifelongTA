#!/usr/bin/env bash
set -euo pipefail

# Extend only Table 2 cells that were competitive in the paired five-seed screen.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_portable_taskmatcher_free"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local f=$1 n=$2 seed=$3 output log input
  input="$ROOT/instances/problem_${n}_${f}_${seed}.json"
  output="$OUT_ROOT/f${f}_n${n}/taskmatcher_free_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then
    return
  fi
  echo "Running free-reassignment TaskMatcher f=$f n=$n seed=$seed"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 3 --matcherDistWeight 10 --matcherTopK 100 \
    --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 >"$log" 2>&1
}

for spec in '2 80' '2 100' '5 50' '10 50' '10 100'; do
  read -r f n <<<"$spec"
  for seed in $(seq 5 24); do
    run_one "$f" "$n" "$seed"
  done
done

printf '\n\a========== TaskMatcher Table 2 free-reassignment expansion complete ==========\n'
date '+Completed at %F %T %Z'
wall 'TaskMatcher Table 2 free-reassignment expansion has completed.' 2>/dev/null || true
