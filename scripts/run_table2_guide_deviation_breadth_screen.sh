#!/usr/bin/env bash
set -euo pipefail

# Broad, low-seed screen before any further confirmation campaign. All nine
# Table 2 release/agent rows use the same released instances and planner.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_ROOT="$ROOT/results/table2_guide_deviation_breadth_screen"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }

run_one() {
  local method=$1
  local f=$2
  local n=$3
  local seed=$4
  local output="$OUT_ROOT/f${f}_n${n}/$method/${method}_${seed}.json"
  mkdir -p "$(dirname "$output")"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -eq 500 ]]; then return; fi
  echo "[$(date '+%F %T %Z')] $method f=$f n=$n seed=$seed"
  if [[ "$method" == flow_unit ]]; then
    "$BIN" --inputFile "$ROOT/instances/problem_${n}_${f}_${seed}.json" --output "$output" \
      --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 1 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
      >"${output%.json}.log" 2>&1
  else
    "$BIN" --inputFile "$ROOT/instances/problem_${n}_${f}_${seed}.json" --output "$output" \
      --outputScreen 3 --simulationTime 400 --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 3 \
      --matcherLexicographicPickup true --matcherGuideDeviationWeight 0.5 \
      --matcherTopK 100 --matcherMaxMatrix 2000000 --matcherReassign true \
      --heapKeepBias 0 --heapProtectDist 0 >"${output%.json}.log" 2>&1
  fi
}

for f in 2 5 10; do
  for n in 50 80 100; do
    for seed in 0 1 2; do
      run_one flow_unit "$f" "$n" "$seed"
      run_one guide_050 "$f" "$n" "$seed"
    done
  done
done

printf '\a========== Table 2 guide-deviation breadth screen complete ==========\n'
date '+Completed at %F %T %Z'
printf '\aTable 2 guide-deviation breadth screening results are ready.\n' | wall 2>/dev/null || true
