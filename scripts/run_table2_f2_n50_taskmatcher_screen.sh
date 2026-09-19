#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
INPUT_ROOT=${INPUT_ROOT:-"$ROOT/instances/table2"}
OUT_DIR="$ROOT/results/table2_f2_n50_taskmatcher_screen"
TIMESTEPS=${1:-400}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
[[ -f "$INPUT_ROOT/problem_50_2_0.json" ]] || { echo "Missing Table 2 inputs: $INPUT_ROOT" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local weight=$1 ratio=$2 seed=$3 tag output log
  tag="w${weight}_r${ratio//./p}"
  output="$OUT_DIR/$tag/seed_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]] &&
     [[ $(jq -r '.numPlannerErrors // -1' "$output") -eq 0 ]] &&
     [[ $(jq -r '.numScheduleErrors // -1' "$output") -eq 0 ]] &&
     [[ $(jq -r '.numEntryTimeouts // -1' "$output") -eq 0 ]]; then
    return
  fi

  "$BIN" --inputFile "$INPUT_ROOT/problem_50_2_${seed}.json" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --finiteTaskStream true --commitWindow 1 \
    --logDetailLevel 2 --matcherDistWeight "$weight" --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherMaxAssign "$ratio" --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
    --matcherUseTraffic false >"$log" 2>&1
}

for weight in 5 10 15; do
  for ratio in 0.5 0.75 1.0; do
    for seed in 0 1 2; do
      run_one "$weight" "$ratio" "$seed"
    done
  done
done
