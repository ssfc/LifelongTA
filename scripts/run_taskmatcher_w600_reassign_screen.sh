#!/usr/bin/env bash
set -euo pipefail

# Screen TaskMatcher reassignment policy on the only Table 1 row still below Flow.
# This does not modify the planner.  The emitted JSON is resumable: completed
# configurations are skipped when this script is re-run.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN=${BIN:-"$ROOT/build/lifelong"}
OUT_DIR="$ROOT/results/portable_assignment/w600_reassign_screen"
TIMESTEPS=${1:-100}
INPUT="$ROOT/instances/warehouseSmall/warehouseSmall_600.json"

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local bias=$1 protect=$2 output log
  output="$OUT_DIR/w600_bias${bias}_protect${protect}_t${TIMESTEPS}.json"
  log="${output%.json}.log"
  if [[ -s "$output" ]] && [[ $(jq -r '.numTaskFinished // 0' "$output") -gt 0 ]]; then
    return
  fi

  echo "Running W600: keep bias=$bias, protect distance=$protect"
  "$BIN" --inputFile "$INPUT" --output "$output" --outputScreen 3 \
    --simulationTime "$TIMESTEPS" --planTimeLimit 1000 --preprocessTimeLimit 30000 \
    --scheduleModel 7 --useTraffic false --assignNew false --commitWindow 1 \
    --logDetailLevel 2 --matcherDistWeight 10 --matcherTopK 100 \
    --matcherMaxMatrix 2000000 --matcherReassign true \
    --heapKeepBias "$bias" --heapProtectDist "$protect" >"$log" 2>&1
}

# Includes the current Table 1 policy (bias=0, protect=0), the original stable
# policy (bias=6, protect=10), and a coarse neighbourhood.  At the one-second
# planning limit, this screen is deliberately limited to 12 configurations.
for bias in 0 2 6 12; do
  for protect in 0 10 20; do
    run_one "$bias" "$protect"
  done
done

printf '\n\a========== W600 TaskMatcher reassignment screen complete ==========\n'
date '+Completed at %F %T %Z'
