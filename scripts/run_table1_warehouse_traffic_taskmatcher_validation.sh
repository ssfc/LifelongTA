#!/usr/bin/env bash
set -euo pipefail

# Same-machine paired validation of the dormitory traffic-aware TaskMatcher on
# the three Warehouse Small rows where the static matcher trailed Flow-Traffic.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_warehouse_traffic_taskmatcher_validation"
TIMESTEPS=${1:-1000}
TRIALS=${2:-3}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

valid_result() {
  local output=$1
  [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and has("numPlannerErrors") and has("numScheduleErrors")' "$output" >/dev/null
}

run_one() {
  local agents=$1 method=$2 trial=$3
  local input="$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json"
  local output="$OUT_DIR/warehouse-small_${agents}_${method}_trial${trial}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  valid_result "$output" && return

  printf '[%s] %s warehouse-small n=%s trial=%s\n' "$(date '+%F %T')" "$method" "$agents" "$trial"
  case "$method" in
    flow_traffic)
      "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
        --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
        --scheduleModel 1 --useTraffic true --assignNew false --commitWindow 1 \
        --logDetailLevel 2 >"$log" 2>&1
      ;;
    taskmatcher_traffic_k100)
      "$BIN" --inputFile "$input" --output "$output" --outputScreen 3 \
        --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
        --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
        --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
        --matcherUseTraffic true --matcherTrafficTopK 100 --matcherTrafficCongestionWeight 1 \
        --matcherTrafficServiceWeight 0 --matcherMaxAssign 1.0 \
        --matcherReassign true --heapKeepBias 0 --heapProtectDist 0 --logDetailLevel 2 >"$log" 2>&1
      ;;
    *) echo "Unknown method: $method" >&2; return 2 ;;
  esac
  valid_result "$output" || { echo "Missing or malformed result: $output" >&2; return 1; }
}

for agents in 400 500 600; do
  for trial in $(seq 1 "$TRIALS"); do
    run_one "$agents" flow_traffic "$trial"
    run_one "$agents" taskmatcher_traffic_k100 "$trial"
  done
done

printf '\n\a========== Warehouse traffic TaskMatcher validation complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Warehouse traffic-aware TaskMatcher validation is complete.' 2>/dev/null || true
