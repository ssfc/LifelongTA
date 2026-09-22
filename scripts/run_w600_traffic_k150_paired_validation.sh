#!/usr/bin/env bash
set -euo pipefail

# Five paired W600 repeats for the selected traffic-aware TaskMatcher setting.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR=${OUT_DIR:-"$ROOT/results/portable_assignment/w600_traffic_k150_paired_validation"}
TIMESTEPS=${1:-1000}
TRIALS=${TRIALS:-5}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_case() {
  local method="$1"
  local trial="$2"
  local output="$OUT_DIR/w600_${method}_t${TIMESTEPS}_trial${trial}.json"
  local log="${output%.json}.log"

  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    echo "Skipping completed $method trial $trial"
    return
  fi

  local args=(
    --inputFile "$ROOT/instances/warehouseSmall/warehouseSmall_600.json"
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS"
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --assignNew false
    --logDetailLevel 2
  )

  if [[ "$method" == "flow-traffic" ]]; then
    args+=(--scheduleModel 1 --useTraffic true)
  else
    args+=(
      --scheduleModel 7 --useTraffic false --commitWindow 1
      --matcherDistWeight 10 --matcherTopK 100 --matcherMaxMatrix 2000000
      --matcherUseTraffic true --matcherTrafficTopK 150
      --matcherTrafficCongestionWeight 1.0 --matcherReassign true
      --heapKeepBias 0 --heapProtectDist 0
    )
  fi

  echo "Running $method trial $trial"
  "$BIN" "${args[@]}" >"$log" 2>&1
}

for ((trial = 1; trial <= TRIALS; ++trial)); do
  run_case flow-traffic "$trial"
  run_case taskmatcher-traffic-k150 "$trial"
done
