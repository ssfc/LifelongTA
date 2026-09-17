#!/usr/bin/env bash
set -euo pipefail

# Five-seed screen before committing compute to the full 25-seed matrix.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_portable_taskmatcher"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local f=$1 n=$2 seed=$3 output log input
  input="$ROOT/instances/problem_${n}_${f}_${seed}.json"
  output="$OUT_ROOT/f${f}_n${n}/taskmatcher_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then
    return
  fi
  echo "Running TaskMatcher screen f=$f n=$n seed=$seed"
  "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 3 --matcherDistWeight 10 --matcherTopK 100 \
    --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias 6 --heapProtectDist 10 >"$log" 2>&1
}

for f in 2 5 10; do
  for n in 50 80 100; do
    for seed in $(seq 0 4); do
      run_one "$f" "$n" "$seed"
    done
  done
done

printf '\n\a========== TaskMatcher Table 2 screening complete ==========\n'
date '+Completed at %F %T %Z'
wall 'TaskMatcher Table 2 five-seed screening has completed.' 2>/dev/null || true
if command -v notify-send >/dev/null 2>&1; then
  notify-send --urgency=normal 'TaskMatcher experiment complete' \
    'The five-seed Table 2 screening has finished.' || true
fi
