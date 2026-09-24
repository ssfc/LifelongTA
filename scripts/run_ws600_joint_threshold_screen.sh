#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
INPUT="$ROOT/instances/warehouseSmall/warehouseSmall_600.json"
OUT_DIR="$ROOT/results/portable_assignment/ws600_joint_congestion_local"
TIMESTEPS=${1:-250}
FREE_LOAD=${2:-2}
mkdir -p "$OUT_DIR"

for weight in 0.1 0.25; do
  output="$OUT_DIR/ws600_joint${weight}_free${FREE_LOAD}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e --argjson steps "$TIMESTEPS" \
    'has("numTaskFinished") and ((.plannerTimes | length) == $steps)' "$output" >/dev/null; then
    continue
  fi
  printf '[%s] joint weight=%s free load=%s t=%s\n' "$(date '+%F %T')" "$weight" "$FREE_LOAD" "$TIMESTEPS"
  "$BIN" --inputFile "$INPUT" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherUseTraffic true --matcherTrafficTopK 100 \
    --matcherTrafficCongestionWeight 1 --matcherTrafficServiceWeight 0 \
    --matcherJointCongestionWeight "$weight" --matcherJointFreeLoad "$FREE_LOAD" \
    --matcherMaxAssign 1.0 --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
    --logDetailLevel 2 >"$log" 2>&1
  jq -e --argjson steps "$TIMESTEPS" '(.plannerTimes | length) == $steps' "$output" >/dev/null
  jq -r '"finished=\(.numTaskFinished) entryTimeouts=\(.numEntryTimeouts) plannerErrors=\(.numPlannerErrors) scheduleErrors=\(.numScheduleErrors)"' "$output"
done
