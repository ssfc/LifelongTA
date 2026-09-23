#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
INPUT="$ROOT/instances/warehouseSmall/warehouseSmall_600.json"
OUT_DIR="$ROOT/results/portable_assignment/ws600_traffic_weight_screen_local"
TIMESTEPS=${1:-250}
TRIALS=${2:-1}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

for trial in $(seq 1 "$TRIALS"); do
  for method in flow_traffic matcher_cw1 matcher_cw2 matcher_cw4; do
    output="$OUT_DIR/ws600_${method}_trial${trial}_t${TIMESTEPS}.json"
    log="${output%.json}.log"
    if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and has("numPlannerErrors") and has("numScheduleErrors")' "$output" >/dev/null; then
      continue
    fi

    args=(--inputFile "$INPUT" --output "$output" --outputScreen 3
      --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000
      --assignNew false --commitWindow 1 --logDetailLevel 2)
    if [[ "$method" == flow_traffic ]]; then
      args+=(--scheduleModel 1 --useTraffic true)
    else
      weight=${method#matcher_cw}
      args+=(--scheduleModel 7 --useTraffic false
        --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000
        --matcherUseTraffic true --matcherTrafficTopK 100
        --matcherTrafficCongestionWeight "$weight" --matcherTrafficServiceWeight 0
        --matcherMaxAssign 1.0 --matcherReassign true
        --heapKeepBias 0 --heapProtectDist 0)
    fi
    printf '[%s] %s trial=%s t=%s\n' "$(date '+%F %T')" "$method" "$trial" "$TIMESTEPS"
    "$BIN" "${args[@]}" >"$log" 2>&1
    jq -r '"finished=\(.numTaskFinished) plannerErrors=\(.numPlannerErrors) scheduleErrors=\(.numScheduleErrors)"' "$output"
  done
done
