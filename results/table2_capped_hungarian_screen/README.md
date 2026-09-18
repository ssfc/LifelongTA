# CappedHungarian: Table 2 Five-Seed Screen

This screen evaluates the portable form of the contest mode-14 Hungarian
scheduler under the unchanged LifelongTA Table 2 finite-workload protocol:
500 tasks, a 400-step cap, and released task streams 0--4.  `n <= 100`, so the
configured 256-agent and 512-task limits do not truncate any candidate set.
The score is agent-to-pickup distance plus pickup-to-delivery distance.  The
scheduler preserves existing assignments and does not produce planner guide
paths or reassign unopened tasks.

All 45 runs finish 500 tasks with zero planner, scheduler, and entry-timeout
errors.  Lower makespan is better.  Delta is CappedHungarian minus Flow; a
positive value favours Flow.  Confidence intervals are paired 95% t intervals
over the five released seeds.

| Release `f` / agents `n` | CappedHungarian | Flow | Delta | Paired 95% CI |
|---|---:|---:|---:|---:|
| 2 / 50 | 355.20 +/- 7.09 | 299.00 +/- 2.92 | 56.20 | [46.18, 66.22] |
| 2 / 80 | 246.00 +/- 10.37 | 240.60 +/- 7.33 | 5.40 | [-7.19, 17.99] |
| 2 / 100 | 222.00 +/- 12.88 | 222.20 +/- 7.76 | -0.20 | [-18.09, 17.69] |
| 5 / 50 | 313.00 +/- 7.75 | 276.00 +/- 9.92 | 37.00 | [17.43, 56.57] |
| 5 / 80 | 230.40 +/- 3.36 | 196.60 +/- 5.13 | 33.80 | [24.13, 43.47] |
| 5 / 100 | 207.00 +/- 12.63 | 168.60 +/- 4.72 | 38.40 | [17.52, 59.28] |
| 10 / 50 | 305.80 +/- 8.26 | 274.80 +/- 7.82 | 31.00 | [19.97, 42.03] |
| 10 / 80 | 219.00 +/- 8.12 | 190.60 +/- 3.91 | 28.40 | [19.42, 37.38] |
| 10 / 100 | 193.20 +/- 11.88 | 168.00 +/- 4.95 | 25.20 | [11.28, 39.12] |

Only `f=2,n=100` is statistically unresolved after screening, but its
five-seed mean advantage is only 0.20 time steps and is not stronger than the
already completed free-reassignment TaskMatcher result on that cell.  The
planned seeds 5--24 expansion was deliberately cancelled; no cell merits a
25-seed CappedHungarian expansion.
