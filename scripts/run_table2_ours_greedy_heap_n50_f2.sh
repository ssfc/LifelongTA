#!/usr/bin/env bash
set -u

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/ours_greedy_heap"
JOBS=${1:-8}

mkdir -p "$OUT_DIR"

run_one() {
  local seed=$1
  local output="$OUT_DIR/problem_50_2_${seed}.json"
  local log="$OUT_DIR/problem_50_2_${seed}.log"

  if [[ -f "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -ge 500 ]]; then
    return 0
  fi

  "$BIN" \
    --inputFile "$ROOT/instances/problem_50_2_${seed}.json" \
    --output "$output" \
    --simulationTime 400 \
    --planTimeLimit 1000 \
    --preprocessTimeLimit 30000 \
    --scheduleModel 6 \
    --heapDistWeight 5 \
    --useTraffic 0 \
    --assignNew 1 \
    --commitWindow 1 \
    --outputScreen 3 \
    --logDetailLevel 3 >"$log" 2>&1
}

for seed in $(seq 0 24); do
  run_one "$seed" &
  while (( $(jobs -pr | wc -l) >= JOBS )); do
    wait -n || true
  done
done

wait
