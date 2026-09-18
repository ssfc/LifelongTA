# TaskMatcher Assignment-Throttling Screen

This screen tests whether the contest assignment-throttling mechanism can close
the external Table 1 gap to Flow-Traffic on Warehouse Small with 400, 500, and
600 agents.  It uses the same portable free-reassignment TaskMatcher setting as
the main external Table 1 result: pickup-distance weight 10, top-100 candidates,
all unopened tasks eligible for rematching, and no old-pair bias or pickup
protection.  Only the maximum assigned-agent ratio `r` changes.

Each run uses the released input, a 1-second planning limit, and 250 time steps.
All 15 runs have zero planner, scheduler, and entry-timeout errors. Higher
throughput is better.

| Agents | `r=0.2` | `r=0.4` | `r=0.6` | `r=0.8` | `r=1.0` |
|---:|---:|---:|---:|---:|---:|
| 400 | 518 | 816 | 1,063 | 1,242 | **1,385** |
| 500 | 596 | 919 | 1,140 | 1,309 | **1,480** |
| 600 | 678 | 978 | 1,143 | 1,349 | **1,436** |

Throughput increases monotonically with the ratio at all three densities.
Consequently, throttling does not help this TaskMatcher configuration and no
1,000-step throttled confirmation is warranted. The existing `r=1.0` main-table
configuration remains the selected setting.
