#!/usr/bin/env bash
set -euo pipefail

# Screen assignment throttling while retaining the traffic-aware TaskMatcher
# costs that are closest to the paper's Flow-Traffic baseline.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR="$ROOT/results/dormitory/traffic_throttling_screen"
TIMESTEPS=${1:-250}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local agents=$1 traffic_k=$2 ratio=$3 ratio_tag=$4 output log
  output="$OUT_DIR/w${agents}_traffic-k${traffic_k}_ratio${ratio_tag}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    return
  fi
  echo "Running W${agents} traffic-k${traffic_k} ratio=${ratio} for ${TIMESTEPS} steps"
  "$BIN" --inputFile "$ROOT/instances/warehouseSmall/warehouseSmall_${agents}.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel 7 \
    --useTraffic false --assignNew false --commitWindow 1 --logDetailLevel 2 \
    --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000 \
    --matcherUseTraffic true --matcherTrafficTopK "$traffic_k" \
    --matcherMaxAssign "$ratio" --matcherReassign true \
    --heapKeepBias 0 --heapProtectDist 0 >"$log" 2>&1
}

for spec in '0.6 60' '0.8 80' '1.0 100'; do
  read -r ratio ratio_tag <<<"$spec"
  run_one 500 25 "$ratio" "$ratio_tag"
  run_one 600 50 "$ratio" "$ratio_tag"
done

printf '\n========== Traffic TaskMatcher throttling screen complete ==========\n'
date '+Completed at %F %T %Z'
