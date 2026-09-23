#!/usr/bin/env bash
set -euo pipefail

# Rebuild the complete local, current-code Flow baseline matrix for all five
# released Sortation Large instances under the strict 1 s timestep budget.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table1_sortation_flow_current"
TIMESTEPS=${1:-1000}

[[ -x "$BIN" ]] || { echo "Missing executable: $BIN" >&2; exit 1; }
mkdir -p "$OUT_DIR"

run_one() {
  local agents=$1 label=$2 model=$3 traffic=$4
  local output="$OUT_DIR/sortation-large_${agents}_${label}_t${TIMESTEPS}.json"
  local log="${output%.json}.log"
  if [[ -s "$output" ]] && jq -e 'has("numTaskFinished") and .error == null' "$output" >/dev/null; then
    return
  fi
  printf '[%s] %s: SL n=%s\n' "$(date '+%F %T')" "$label" "$agents"
  "$BIN" --inputFile "$ROOT/instances/sortationLarge/sortationLarge_${agents}.json" \
    --output "$output" --outputScreen 3 --simulationTime "$TIMESTEPS" \
    --planTimeLimit 1000 --preprocessTimeLimit 30000 --scheduleModel "$model" \
    --useTraffic "$traffic" --assignNew false --commitWindow 1 --logDetailLevel 2 >"$log" 2>&1
}

for agents in 4000 8000 12000 16000 20000; do
  run_one "$agents" flow_unit 1 false
  run_one "$agents" flow_traffic 1 true
  run_one "$agents" flow_avg_waiting 2 false
done

printf '\n\a========== Current-code Sortation Large Flow matrix complete ==========\n'
date '+Completed at %F %T %Z'
wall 'Current-code Sortation Large Flow baseline matrix has completed.' 2>/dev/null || true
