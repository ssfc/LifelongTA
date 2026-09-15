#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RUNNER="$ROOT/scripts/run_table2_portable_serial.sh"
BASE="$ROOT/results/table2_portable_heap"
STATUS="$BASE/campaign_status.txt"
TARGETS=('5 100' '10 50' '10 80' '10 100')

count_completed() {
  local f=$1 n=$2 method=$3 dir="$BASE/f${f}_n${n}"
  if [[ ! -d "$dir" ]]; then
    printf '0'
    return
  fi
  find "$dir" -maxdepth 1 -name "${method}_*.json" -print | wc -l | tr -d ' '
}

write_status() {
  local state=$1 f=$2 n=$3
  mkdir -p "$BASE"
  {
    printf 'updated: %s\n' "$(date -Is)"
    printf 'state: %s\n' "$state"
    printf 'current target: f=%s, n=%s\n' "$f" "$n"
    printf 'completed seeds: greedy=%s/25, flow=%s/25, heap=%s/25\n' \
      "$(count_completed "$f" "$n" greedy)" \
      "$(count_completed "$f" "$n" flow)" \
      "$(count_completed "$f" "$n" heap)"
    printf 'campaign targets: f=5,n=100; f=10,n=50; f=10,n=80; f=10,n=100\n'
  } >"$STATUS"
}

wait_for_existing_f5_n100() {
  while pgrep -f 'run_table2_portable_serial\.sh 5 100' >/dev/null; do
    write_status 'waiting for already-running f=5,n=100 batch' 5 100
    sleep 60
  done
}

run_target() {
  local f=$1 n=$2 child
  write_status 'running' "$f" "$n"
  "$RUNNER" "$f" "$n" &
  child=$!
  while kill -0 "$child" 2>/dev/null; do
    write_status 'running' "$f" "$n"
    sleep 60
  done
  wait "$child"
  write_status 'completed' "$f" "$n"
}

wait_for_existing_f5_n100
for target in "${TARGETS[@]}"; do
  read -r f n <<<"$target"
  run_target "$f" "$n"
done

write_status 'campaign completed' 10 100
