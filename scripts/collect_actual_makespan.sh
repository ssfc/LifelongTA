#!/usr/bin/env bash
set -euo pipefail

# Re-score finite-workload outputs from task completion timestamps. Historical
# JSON makespan is retained only as maxAgentActiveSteps and is not read here.
ROOT=${1:?usage: collect_actual_makespan.sh RESULTS_DIR OUTPUT_CSV}
OUTPUT=${2:?usage: collect_actual_makespan.sh RESULTS_DIR OUTPUT_CSV}

mkdir -p "$(dirname "$OUTPUT")"
printf 'result_json,task_metrics,status,tasks,completed_tasks,actual_makespan\n' >"$OUTPUT"

while IFS= read -r result; do
  metrics="${result%.json}.task_metrics.csv"
  if [[ ! -f "$metrics" ]]; then
    printf '%s,%s,missing_metrics,,,\n' "$result" "$metrics" >>"$OUTPUT"
    continue
  fi
  summary=$(awk -F, '
    NR == 1 { for (i = 1; i <= NF; ++i) if ($i == "completed_at") completed_col = i; next }
    completed_col > 0 { tasks += 1; if ($completed_col >= 0) { completed += 1; if ($completed_col > last) last = $completed_col } }
    END { if (completed_col == 0) print "invalid_header,,,"; else if (tasks == completed) printf "complete,%d,%d,%d", tasks, completed, last; else printf "incomplete,%d,%d,", tasks, completed }
  ' "$metrics")
  printf '%s,%s,%s\n' "$result" "$metrics" "$summary" >>"$OUTPUT"
done < <(find "$ROOT" -type f -name '*.json' ! -name '*.metrics_summary.json' | sort)

printf 'Wrote %s\n' "$OUTPUT"
