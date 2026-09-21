#!/usr/bin/env python3
"""Summarize finite-workload Table 2 outputs using actual task completion time."""

import csv
import json
import math
import re
import sys
from collections import defaultdict
from pathlib import Path


def sample_sd(values: list[int]) -> float:
    if len(values) < 2:
        return 0.0
    mean = sum(values) / len(values)
    return math.sqrt(sum((value - mean) ** 2 for value in values) / (len(values) - 1))


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Usage: {Path(sys.argv[0]).name} RESULTS_DIR OUTPUT_CSV", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]).resolve()
    output = Path(sys.argv[2]).resolve()
    groups: dict[tuple[int, int, str], list[int]] = defaultdict(list)
    skipped: list[tuple[Path, str]] = []
    pattern = re.compile(r"f(\d+)_n(\d+)/(.*)_(\d+)\.json$")

    for result in sorted(root.rglob("*.json")):
        if result.name.endswith(".metrics_summary.json"):
            continue
        match = pattern.search(result.relative_to(root).as_posix())
        if not match:
            continue
        f, n, method, _seed = match.groups()
        try:
            data = json.loads(result.read_text())
        except json.JSONDecodeError:
            skipped.append((result, "invalid_json"))
            continue
        makespan = data.get("actualMakespan")
        if data.get("numTaskFinished") != 500 or data.get("allTasksCompleted") is not True:
            skipped.append((result, "incomplete_workload"))
        elif not isinstance(makespan, int) or makespan < 0:
            skipped.append((result, "missing_actual_makespan"))
        else:
            groups[(int(f), int(n), method)].append(makespan)

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(("release_f", "agents", "method", "runs", "mean_actual_makespan", "sample_sd"))
        for (f, n, method), values in sorted(groups.items()):
            writer.writerow((f, n, method, len(values), f"{sum(values) / len(values):.3f}", f"{sample_sd(values):.3f}"))

    skipped_path = output.with_name(output.stem + "_skipped.csv")
    with skipped_path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(("result_json", "reason"))
        writer.writerows((str(path), reason) for path, reason in skipped)

    print(f"Wrote {output} ({sum(map(len, groups.values()))} valid runs)")
    if skipped:
        print(f"Skipped {len(skipped)} outputs; details: {skipped_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
