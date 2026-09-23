# Warehouse Small, 600 agents: local scheduler screen

All runs use `instances/warehouseSmall/warehouseSmall_600.json`, 1,000 ms planning budget per timestep, the existing planner and task stream, and serial execution on this machine. The TaskMatcher settings match `scripts/run_table1_warehouse_traffic_taskmatcher_validation.sh` except for the traffic congestion weight. These are single 250-timestep screening runs, not a repeated long-horizon comparison.

| Scheduler | Congestion weight | Finished tasks | Errors |
| --- | ---: | ---: | ---: |
| Flow-Traffic | N/A | 1501 | 0 |
| TaskMatcher traffic-k100 | 1 | 1568 | 0 |
| TaskMatcher traffic-k100 | 2 | 1552 | 0 |
| TaskMatcher traffic-k100 | 4 | 1515 | 0 |

Increasing the congestion weight did not improve this local screen. The previously recorded local 1,000-timestep, three-run comparison remains Flow-Traffic `[6226, 6287, 6212]` versus TaskMatcher traffic-k100 `[6080, 6134, 6184]`. The 250-timestep lead must not be interpreted as a long-horizon win.

A separate exploratory run switched from TaskMatcher traffic-k100 to Flow-Traffic at timestep 250. Its 1,000-timestep result was 6062 finished tasks, with zero entry timeouts, planner errors, and schedule errors. This one-off hybrid was not retained in the scheduler code. Its JSON and log are in `../ws600_hybrid_switch_local/`.
