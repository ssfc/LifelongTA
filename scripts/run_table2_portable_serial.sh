#!/usr/bin/env bash
set -u

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <release-rate-f> <agents-n>" >&2
  exit 2
fi

F=$1
N=$2
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table2_portable_heap/f${F}_n${N}"

if [[ ! -x "$BIN" ]]; then
  echo "Missing executable: $BIN" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

run_one() {
  local method=$1
  local model=$2
  local assign_new=$3
  local seed=$4
  local stem="problem_${N}_${F}_${seed}"
  local output="$OUT_DIR/${method}_${seed}.json"
  local log="$OUT_DIR/${method}_${seed}.log"

  if [[ -f "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -ge 500 ]]; then
    return 0
  fi

  "$BIN" \
    --inputFile "$ROOT/instances/${stem}.json" \
    --output "$output" \
    --simulationTime 400 \
    --planTimeLimit 1000 \
    --preprocessTimeLimit 30000 \
    --scheduleModel "$model" \
    --useTraffic 0 \
    --assignNew "$assign_new" \
    --commitWindow 1 \
    --outputScreen 3 \
    --logDetailLevel 3 \
    --heapDistWeight 5 \
    --heapReassign 1 \
    --heapKeepBias 6 \
    --heapProtectDist 10 \
    --heapRebuildPct 45 \
    --heapLnsPct 10 \
    --heapSortK 500 >"$log" 2>&1
}

for spec in 'greedy 5 1' 'flow 1 0' 'heap 6 1'; do
  read -r method model assign_new <<<"$spec"
  for seed in $(seq 0 24); do
    run_one "$method" "$model" "$assign_new" "$seed"
  done
done
