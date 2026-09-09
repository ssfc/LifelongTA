#!/usr/bin/env bash
set -euo pipefail

# Reproduce the strict 1-second-per-timestep variants in Table 1 of
# "Flow-Based Task Assignment for Large-Scale Online MAPD".
usage() {
    echo "Usage: $0 <random|warehouse-small|sortation-large> <team-size> <greedy|flow-unit|flow-traffic|flow-avg-waiting> [simulation-timesteps]"
    exit 2
}

[[ $# -ge 3 && $# -le 4 ]] || usage

map_family="$1"
team_size="$2"
method="$3"
timesteps="${4:-1000}"

case "$map_family" in
    random)
        input_file="instances/random/random_${team_size}.json"
        ;;
    warehouse-small)
        input_file="instances/warehouseSmall/warehouseSmall_${team_size}.json"
        ;;
    sortation-large)
        input_file="instances/sortationLarge/sortationLarge_${team_size}.json"
        ;;
    *) usage ;;
esac

[[ -f "$input_file" ]] || { echo "Missing instance: $input_file" >&2; exit 1; }

case "$method" in
    greedy)
        schedule_model=5
        use_traffic=false
        ;;
    flow-unit)
        schedule_model=1
        use_traffic=false
        ;;
    flow-traffic)
        schedule_model=1
        use_traffic=true
        ;;
    flow-avg-waiting)
        schedule_model=2
        use_traffic=false
        ;;
    *) usage ;;
esac

mkdir -p results/table1_strict
output="results/table1_strict/${map_family}_${team_size}_${method}_${timesteps}.json"

./build/lifelong \
    --inputFile "$input_file" \
    --output "$output" \
    --outputScreen 3 \
    --simulationTime "$timesteps" \
    --planTimeLimit 1000 \
    --preprocessTimeLimit 30000 \
    --scheduleModel "$schedule_model" \
    --useTraffic "$use_traffic" \
    --assignNew false \
    --logDetailLevel 2

echo "Saved result: $output"
