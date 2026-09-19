#!/usr/bin/env bash
set -euo pipefail

# Paired finite-workload confirmation for the eight Table 2 cells other than
# f=2,n=50, which already has a completed five-seed comparison.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build-ucrt/lifelong.exe"}
INPUT_ROOT=${INPUT_ROOT:-"$ROOT/instances/table2"}
OUT_ROOT="$ROOT/results/table2_fixed_paired"
TIMESTEPS=${1:-400}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
[[ -f "$INPUT_ROOT/problem_80_5_0.json" ]] || { echo "Missing Table 2 inputs: $INPUT_ROOT" >&2; exit 1; }
mkdir -p "$OUT_ROOT"

is_valid() {
  local output=$1
  [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]] &&
    [[ $(jq -r '.numPlannerErrors // -1' "$output") -eq 0 ]] &&
    [[ $(jq -r '.numScheduleErrors // -1' "$output") -eq 0 ]] &&
    [[ $(jq -r '.numEntryTimeouts // -1' "$output") -eq 0 ]]
}

run_one() {
  local scheduler=$1 f=$2 n=$3 seed=$4 label output log input
  label=$([[ "$scheduler" == "flow" ]] && echo flow_unit || echo taskmatcher_fixed)
  input="$INPUT_ROOT/problem_${n}_${f}_${seed}.json"
  output="$OUT_ROOT/f${f}_n${n}/${label}_${seed}.json"
  log="${output%.json}.log"
  mkdir -p "$(dirname "$output")"
  [[ -f "$input" ]] || { echo "Missing input: $input" >&2; return 2; }
  is_valid "$output" && return

  echo "Running $label f=$f n=$n seed=$seed"
  if [[ "$scheduler" == "flow" ]]; then
    "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
      --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 1 --useTraffic false --assignNew false --finiteTaskStream true --commitWindow 1 \
      --logDetailLevel 2 >"$log" 2>&1
  else
    "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
      --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 7 --useTraffic false --assignNew false --finiteTaskStream true --commitWindow 1 \
      --logDetailLevel 2 --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
      --matcherMaxAssign 0.5 --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
      --matcherUseTraffic false >"$log" 2>&1
  fi
}

for f in 2 5 10; do
  for n in 50 80 100; do
    [[ "$f" == 2 && "$n" == 50 ]] && continue
    for seed in 0 1 2 3 4; do
      run_one flow "$f" "$n" "$seed"
      run_one taskmatcher "$f" "$n" "$seed"
    done
  done
done

printf '\n========== Fixed paired Table 2 campaign complete ==========\n'
date '+Completed at %F %T %Z'
