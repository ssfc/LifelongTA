# StableSnatchHungarian: Table 2 Five-Seed Screen

This screen evaluates a portable form of the contest stable-snatch rule under
the unchanged LifelongTA Table 2 finite-workload protocol: 500 tasks, a
400-step cap, and released task streams 0--4.  The scheduler uses full
Hungarian matching at these sizes. An unopened incumbent task may be reassigned
only when its pickup is more than 10 steps away and another agent lowers its
individual cost by at least `max(6, 10% of incumbent cost)`.

All 45 runs finish 500 tasks with zero planner, scheduler, and entry-timeout
errors. Lower makespan is better. Delta is StableSnatchHungarian minus
Flow-Unit; a positive value favours Flow-Unit. Confidence intervals are paired
95% t intervals over the five released seeds.

| Release `f` / agents `n` | StableSnatch | CappedHungarian | Flow-Unit | Delta vs Flow-Unit | Paired 95% CI |
|---|---:|---:|---:|---:|---:|
| 2 / 50 | 323.80 +/- 2.28 | 355.20 +/- 7.09 | 299.00 +/- 2.92 | 24.80 | [21.03, 28.57] |
| 2 / 80 | 244.80 +/- 5.63 | 246.00 +/- 10.37 | 240.60 +/- 7.33 | 4.20 | [-2.62, 11.02] |
| 2 / 100 | 235.60 +/- 17.14 | 222.00 +/- 12.88 | 222.20 +/- 7.76 | 13.40 | [-16.35, 43.15] |
| 5 / 50 | 297.20 +/- 4.49 | 313.00 +/- 7.75 | 276.00 +/- 9.92 | 21.20 | [11.57, 30.83] |
| 5 / 80 | 217.20 +/- 3.56 | 230.40 +/- 3.36 | 196.60 +/- 5.13 | 20.60 | [11.37, 29.83] |
| 5 / 100 | 187.20 +/- 6.10 | 207.00 +/- 12.63 | 168.60 +/- 4.72 | 18.60 | [11.60, 25.60] |
| 10 / 50 | 293.80 +/- 5.40 | 305.80 +/- 8.26 | 274.80 +/- 7.82 | 19.00 | [8.18, 29.82] |
| 10 / 80 | 206.60 +/- 3.05 | 219.00 +/- 8.12 | 190.60 +/- 3.91 | 16.00 | [8.92, 23.08] |
| 10 / 100 | 179.80 +/- 2.59 | 193.20 +/- 11.88 | 168.00 +/- 4.95 | 11.80 | [4.81, 18.79] |

Stable-snatch improves on CappedHungarian in eight of nine sample means, but
does not beat Flow-Unit in any cell.  No 25-seed expansion is warranted.
