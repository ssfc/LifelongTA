#!/usr/bin/env bash
set -u

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BIN="$ROOT/build/lifelong"
OUT_DIR="$ROOT/results/table3"

mkdir -p "$OUT_DIR"

run_one() {
  local label=$1
  local input=$2
  local steps=$3
  local window=$4
  local model=$5
  local stem="${label}_$( [ "$model" -eq 1 ] && echo flow || echo greedy )"
  local output="$OUT_DIR/${stem}.json"
  local log="$OUT_DIR/${stem}.log"

  if [[ -f "$output" ]]; then
    return 0
  fi

  "$BIN" \
    --inputFile "$ROOT/$input" \
    --output "$output" \
    --simulationTime "$steps" \
    --planTimeLimit 1000 \
    --preprocessTimeLimit 30000 \
    --scheduleModel "$model" \
    --useTraffic 0 \
    --assignNew 0 \
    --commitWindow "$window" \
    --outputScreen 3 \
    --logDetailLevel 3 >"$log" 2>&1
}

case ${1:-all} in
  orz)
    run_one orz_10000 instances/game.domain/game_orz_1_10000.json 2000 10 5
    run_one orz_10000 instances/game.domain/game_orz_1_10000.json 2000 10 1
    run_one orz_20000 instances/game.domain/game_orz_1_20000.json 2000 10 5
    run_one orz_20000 instances/game.domain/game_orz_1_20000.json 2000 10 1
    ;;
  ih)
    run_one ih_10000 example_problems/ih.domain/IHLargeTest_10000.json 5000 30 5
    run_one ih_10000 example_problems/ih.domain/IHLargeTest_10000.json 5000 30 1
    run_one ih_20000 example_problems/ih.domain/IHLargeTest_20000.json 5000 30 5
    run_one ih_20000 example_problems/ih.domain/IHLargeTest_20000.json 5000 30 1
    ;;
  all)
    "$0" orz
    "$0" ih
    ;;
  *)
    echo "usage: $0 [orz|ih|all]" >&2
    exit 1
    ;;
esac
