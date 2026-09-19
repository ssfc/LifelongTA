#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
INPUT_ROOT=${INPUT_ROOT:-"$ROOT/instances/table2"}
OUT_DIR="$ROOT/results/table2_f2_n50_flow_unit"
TIMESTEPS=${1:-400}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
[[ -f "$INPUT_ROOT/problem_50_2_0.json" ]] || { echo "Missing Table 2 inputs: $INPUT_ROOT" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local seed=$1 output log
  output="$OUT_DIR/flow_unit_${seed}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]] &&
     [[ $(jq -r '.numScheduleErrors // -1' "$output") -eq 0 ]]; then
    return
  fi

  "$BIN" --inputFile "$INPUT_ROOT/problem_50_2_${seed}.json" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 1 --useTraffic false --assignNew false --finiteTaskStream true --commitWindow 1 \
    --logDetailLevel 2 >"$log" 2>&1
}

for seed in 0 1 2 3 4; do
  run_one "$seed"
done
