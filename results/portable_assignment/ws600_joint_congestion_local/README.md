# WS600 joint-congestion TaskMatcher experiment

All complete runs use `instances/warehouseSmall/warehouseSmall_600.json`, the existing LifelongTA planner and task stream, a 1,000 ms per-timestep budget, and serial execution on this machine. The scheduler is the traffic-k100 TaskMatcher with `matcherDistWeight=10`, free unopened-task reassignment, and no assignment throttling. Only the joint-congestion options differ. The two-pass scheduler is experimental and disabled by default.

The second pass estimates load from the first assignment's agent-to-pickup routes, then adds a cost for other agents' predicted routes through a vertex. `free load` is the number of other predicted routes allowed before this extra cost applies. It does not model *when* agents reach the vertex.

| Timesteps | Joint weight | Free load | Finished tasks | Entry/planner/schedule errors |
| ---: | ---: | ---: | ---: | ---: |
| 250 | 0 | 0 | 1553 | 0/0/0 |
| 250 | 0.025 | 0 | 1543 | 0/0/0 |
| 250 | 0.05 | 0 | 1503 | 0/0/0 |
| 250 | 0.1 | 0 | 1520 | 0/0/0 |
| 250 | 0.25 | 0 | 1410 | 0/0/0 |
| 250 | 0.1 | 2 | 1554 | 0/0/0 |
| 250 | 0.25 | 2 | 1522 | 0/0/0 |
| 1000 | 0.025 | 0 | 6154 | 0/0/0 |
| 1000 | 0.1 | 2 | 6064 | 0/0/0 |

The local three-run 1,000-timestep reference remains Flow-Traffic `[6226, 6287, 6212]` (mean 6241.7) and the original traffic-k100 TaskMatcher `[6080, 6134, 6184]` (mean 6132.7), from `../../table1_warehouse_traffic_taskmatcher_validation/`. Neither joint-congestion candidate reached even the lowest Flow-Traffic run. The 6154 result is a single run and falls within the original TaskMatcher's observed range; it is not evidence of improvement.

The `*.interrupted.json` files contain only 50 and 9 timesteps, respectively, and are **not** experimental results. They were produced when the stronger-weight screen was stopped after weight 0.25 had already shown a large loss.

Conclusion: geometric overlap of predicted pickup routes is too coarse a congestion proxy here. Penalizing it strongly harms throughput; allowing two routes to overlap did not rescue the 1,000-timestep result. A future variant would need time-aware load or measured planner waiting, rather than a larger weight sweep on this proxy.
