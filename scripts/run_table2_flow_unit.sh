#!/usr/bin/env bash
set -u

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table2_flow_unit"
JOBS=${1:-8}
STEPS=${2:-400}

mkdir -p "$OUT_DIR"

run_one() {
  local n=$1
  local f=$2
  local seed=$3
  local stem="problem_${n}_${f}_${seed}"
  local output="$OUT_DIR/${stem}.json"
  local log="$OUT_DIR/${stem}.log"

  if [[ -f "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -ge 500 ]]; then
    return 0
  fi

  "$BIN" \
    --inputFile "$ROOT/instances/${stem}.json" \
    --output "$output" \
    --simulationTime "$STEPS" \
    --planTimeLimit 1000 \
    --preprocessTimeLimit 30000 \
    --scheduleModel 1 \
    --useTraffic 0 \
    --assignNew 0 \
    --commitWindow 1 \
    --outputScreen 3 \
    --logDetailLevel 3 >"$log" 2>&1
}

if [[ ! -x "$BIN" ]]; then
  echo "Missing executable: $BIN" >&2
  exit 1
fi

for f in 2 5 10; do
  for n in 50 80 100; do
    for seed in $(seq 0 24); do
      run_one "$n" "$f" "$seed" &
      while (( $(jobs -pr | wc -l) >= JOBS )); do
        wait -n || true
      done
    done
  done
done

wait
