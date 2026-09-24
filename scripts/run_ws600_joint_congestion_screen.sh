#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
INPUT="$ROOT/instances/warehouseSmall/warehouseSmall_600.json"
OUT_DIR="$ROOT/results/portable_assignment/ws600_joint_congestion_local"
TIMESTEPS=${1:-250}
TRIALS=${2:-1}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for trial in $(seq 1 "$TRIALS"); do
  for weight in 0 0.025 0.05 0.1; do
    output="$OUT_DIR/ws600_joint${weight}_trial${trial}_t${TIMESTEPS}.json"
    log="${output%.json}.log"
    if [[ -s "$output" ]] && jq -e --argjson steps "$TIMESTEPS" \
      'has("numTaskFinished") and has("numPlannerErrors") and has("numScheduleErrors") and ((.plannerTimes | length) == $steps)' \
      "$output" >/dev/null; then
      continue
    fi
    printf '[%s] joint weight=%s trial=%s t=%s\n' "$(date '+%F %T')" "$weight" "$trial" "$TIMESTEPS"
    "$BIN" --inputFile "$INPUT" --output "$output" --outputScreen 3 \
      --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
      --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
      --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
      --matcherUseTraffic true --matcherTrafficTopK 100 \
      --matcherTrafficCongestionWeight 1 --matcherTrafficServiceWeight 0 \
      --matcherJointCongestionWeight "$weight" --matcherMaxAssign 1.0 \
      --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 \
      --logDetailLevel 2 >"$log" 2>&1
    jq -e --argjson steps "$TIMESTEPS" '(.plannerTimes | length) == $steps' "$output" >/dev/null
    jq -r '"finished=\(.numTaskFinished) entryTimeouts=\(.numEntryTimeouts) plannerErrors=\(.numPlannerErrors) scheduleErrors=\(.numScheduleErrors) steps=\(.plannerTimes | length)"' "$output"
  done
done
