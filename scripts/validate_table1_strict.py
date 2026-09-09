#!/usr/bin/env python3
"""Compare strict-realtime Table 1 runs against the paper's reported throughput."""

from __future__ import annotations

import json
from pathlib import Path


REFERENCE = {
    "random": {
        400: {"greedy": 6925, "flow-unit": 6972, "flow-traffic": 6964, "flow-avg-waiting": 6975},
        800: {"greedy": 12660, "flow-unit": 12826, "flow-traffic": 12745, "flow-avg-waiting": 12787},
        1200: {"greedy": 15874, "flow-unit": 16331, "flow-traffic": 16205, "flow-avg-waiting": 16402},
        1600: {"greedy": 16316, "flow-unit": 16383, "flow-traffic": 17097, "flow-avg-waiting": 17001},
        2000: {"greedy": 15265, "flow-unit": 16101, "flow-traffic": 15939, "flow-avg-waiting": 16111},
    },
    "warehouse-small": {
        200: {"greedy": 3512, "flow-unit": 3642, "flow-traffic": 3596, "flow-avg-waiting": 3632},
        300: {"greedy": 4779, "flow-unit": 4919, "flow-traffic": 4863, "flow-avg-waiting": 4816},
        400: {"greedy": 5494, "flow-unit": 5718, "flow-traffic": 5761, "flow-avg-waiting": 5637},
        500: {"greedy": 5894, "flow-unit": 5829, "flow-traffic": 6174, "flow-avg-waiting": 5819},
        600: {"greedy": 5593, "flow-unit": 5652, "flow-traffic": 6185, "flow-avg-waiting": 5678},
    },
    "sortation-large": {
        4000: {"greedy": 13642, "flow-unit": 13173, "flow-traffic": 13449, "flow-avg-waiting": 14094},
        8000: {"greedy": 25831, "flow-unit": 18507, "flow-traffic": 25569, "flow-avg-waiting": 26177},
        12000: {"greedy": 34414, "flow-unit": 23136, "flow-traffic": 33355, "flow-avg-waiting": 34568},
        16000: {"greedy": 31993, "flow-unit": 26490, "flow-traffic": 34277, "flow-avg-waiting": 33975},
        20000: {"greedy": 27323, "flow-unit": 24501, "flow-traffic": 31658, "flow-avg-waiting": 29477},
    },
}


def main() -> None:
    results_dir = Path("results/table1_strict")
    completed = 0
    for family, sizes in REFERENCE.items():
        for team_size, methods in sizes.items():
            for method, expected in methods.items():
                path = results_dir / f"{family}_{team_size}_{method}_1000.json"
                if not path.exists():
                    continue
                result = json.loads(path.read_text())
                actual = result["numTaskFinished"]
                delta = (actual - expected) / expected * 100.0
                healthy = not any(
                    result[key]
                    for key in ("numEntryTimeouts", "numPlannerErrors", "numScheduleErrors")
                )
                status = "OK" if healthy else "ERROR"
                print(f"{status:5} {family:16} n={team_size:5} {method:16} "
                      f"paper={expected:5} local={actual:5} delta={delta:+6.2f}%")
                completed += 1
    print(f"Validated {completed}/60 strict Table 1 cells.")


if __name__ == "__main__":
    main()
