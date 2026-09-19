#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
INPUT_ROOT=${INPUT_ROOT:-"$ROOT/instances/table2"}
OUT_DIR="$ROOT/results/table2_f2_n50_taskmatcher_best_confirm"
TIMESTEPS=${1:-400}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
[[ -f "$INPUT_ROOT/problem_50_2_3.json" ]] || { echo "Missing Table 2 inputs: $INPUT_ROOT" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for seed in 3 4; do
  output="$OUT_DIR/seed_${seed}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]] &&
     [[ $(jq -r '.numPlannerErrors // -1' "$output") -eq 0 ]] &&
     [[ $(jq -r '.numScheduleErrors // -1' "$output") -eq 0 ]] &&
     [[ $(jq -r '.numEntryTimeouts // -1' "$output") -eq 0 ]]; then
    continue
  fi

  "$BIN" --inputFile "$INPUT_ROOT/problem_50_2_${seed}.json" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --finiteTaskStream true --commitWindow 1 \
    --logDetailLevel 2 --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherMaxAssign 0.5 --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
    --matcherUseTraffic false >"$log" 2>&1
done
