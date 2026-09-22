#!/usr/bin/env bash
set -euo pipefail

# Do not overlap the Table 1 completion with the CPU-heavy corrected Table 2
# campaign. Start only after the latter creates a complete 27-row summary.
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TABLE2_ROOT=/home/ssfc/LifelongTA-rmca-compare/results/table2_actual_rebaseline
SUMMARY="$TABLE2_ROOT/actual_makespan_summary.csv"

while tmux has-session -t table2_actual_rebaseline 2>/dev/null; do
  sleep 60
done

[[ -f "$SUMMARY" ]] || { echo "Table 2 exited without summary: $SUMMARY" >&2; exit 1; }
[[ $(awk 'END { print NR - 1 }' "$SUMMARY") -eq 27 ]] || {
  echo "Table 2 summary is incomplete: $SUMMARY" >&2
  exit 1
}
awk -F, 'NR > 1 && $4 != 25 { exit 1 }' "$SUMMARY" || {
  echo "Table 2 summary does not contain 25 valid runs per method/cell" >&2
  exit 1
}

exec "$ROOT/scripts/run_table1_sortation_ours.sh" 1000
