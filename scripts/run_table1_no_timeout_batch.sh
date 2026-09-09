#!/usr/bin/env bash
set -euo pipefail

for family_and_sizes in \
    'random 400 800 1200 1600 2000' \
    'warehouse-small 200 300 400 500 600' \
    'sortation-large 4000 8000 12000 16000 20000'; do
    read -r family sizes <<<"$family_and_sizes"
    for team_size in $sizes; do
        for method in greedy flow-unit; do
            output="results/table1_no_timeout/${family}_${team_size}_${method}_1000.json"
            [[ -f "$output" ]] && continue
            scripts/run_table1_no_timeout.sh "$family" "$team_size" "$method" 1000
        done
    done
done
