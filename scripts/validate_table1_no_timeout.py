#!/usr/bin/env python3
"""Compare Table 1 no-timeout runs against the paper's reported throughput."""

from __future__ import annotations

import json
from pathlib import Path


REFERENCE = {
    "random": {400: (6936, 6980), 800: (12688, 12871), 1200: (15839, 16211), 1600: (16172, 16971), 2000: (15411, 15626)},
    "warehouse-small": {200: (3508, 3639), 300: (4773, 4954), 400: (5570, 5673), 500: (5791, 5900), 600: (5585, 5836)},
    "sortation-large": {4000: (13776, 14490), 8000: (26355, 27614), 12000: (33952, 35912), 16000: (31428, 33960), 20000: (29029, 31852)},
}
METHODS = (("greedy", 0), ("flow-unit", 1))


def main() -> None:
    results = Path("results/table1_no_timeout")
    deltas = []
    for family, sizes in REFERENCE.items():
        for team_size, expected_values in sizes.items():
            for method, index in METHODS:
                path = results / f"{family}_{team_size}_{method}_1000.json"
                data = json.loads(path.read_text())
                expected = expected_values[index]
                actual = data["numTaskFinished"]
                delta = (actual - expected) / expected * 100.0
                deltas.append(abs(delta))
                errors = sum(data[key] for key in ("numEntryTimeouts", "numPlannerErrors", "numScheduleErrors"))
                print(f"{'OK' if errors == 0 else 'ERROR':5} {family:16} n={team_size:5} {method:9} paper={expected:5} local={actual:5} delta={delta:+6.2f}%")
    print(f"Validated {len(deltas)}/30 no-timeout cells; mean absolute delta={sum(deltas)/len(deltas):.2f}%, max={max(deltas):.2f}%.")


if __name__ == "__main__":
    main()
