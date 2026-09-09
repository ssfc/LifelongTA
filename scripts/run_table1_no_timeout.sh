#!/usr/bin/env bash
set -euo pipefail

[[ $# -ge 3 && $# -le 4 ]] || { echo "Usage: $0 <random|warehouse-small|sortation-large> <team-size> <greedy|flow-unit> [timesteps]" >&2; exit 2; }

family="$1"
team_size="$2"
method="$3"
timesteps="${4:-1000}"

case "$family" in
    random) input="instances/random/random_${team_size}.json" ;;
    warehouse-small) input="instances/warehouseSmall/warehouseSmall_${team_size}.json" ;;
    sortation-large) input="instances/sortationLarge/sortationLarge_${team_size}.json" ;;
    *) exit 2 ;;
esac

case "$method" in
    greedy) model=5 ;;
    flow-unit) model=1 ;;
    *) exit 2 ;;
esac

mkdir -p results/table1_no_timeout
output="results/table1_no_timeout/${family}_${team_size}_${method}_${timesteps}.json"

./build/lifelong --inputFile "$input" --output "$output" --outputScreen 3 \
    --simulationTime "$timesteps" --planTimeLimit 600000 --refinementTimeLimit 1000 \
    --preprocessTimeLimit 30000 --scheduleModel "$model" --useTraffic false \
    --assignNew false --logDetailLevel 2
