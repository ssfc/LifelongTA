# Experiment metrics

Every simulation result written to `result.json` also writes four files beside it:

- `result.task_metrics.csv`: one row per revealed task.
- `result.agent_metrics.csv`: one row per agent.
- `result.timeline_metrics.csv`: one row per simulation timestep.
- `result.metrics_summary.json`: aggregate values for direct comparisons.

## Task lifecycle

`revealed_at`, `first_assigned_at`, `picked_up_at`, and `completed_at` are timesteps. A task without a corresponding event uses `-1`.

- `pickup_distance_at_assignment` is the static shortest-path distance from the agent position at first assignment to the pickup location.
- `service_shortest_distance` is the sum of static shortest-path distances from pickup through all delivery locations.
- `empty_distance` counts movement before the first pickup; `loaded_distance` counts movement after pickup.
- `active_wait_steps` counts timesteps in which an assigned agent did not change grid cell.
- `loaded_detour_ratio = loaded_distance / service_shortest_distance`. Values above one show route detours after pickup.

Latency fields (`assignment_wait`, `pickup_wait`, `completion_time`) are measured from task reveal. They are `-1` for unreached lifecycle events, so unfinished tasks must be considered through `backlogAtEnd` rather than silently excluded.

## Aggregate comparison

For a Flow versus TaskMatcher comparison, report at least:

1. `tasksCompleted`, `unassignedBacklogAtEnd`, and `activeTasksAtEnd`.
2. `arrivalToCompletion` p50/p95/p99, not only its mean.
3. `pickupDistanceAtAssignment` and `emptyDistance`.
4. `loadedDetourRatio` and `productiveMovementRatio`.
5. `activeWait`, plus the distribution of per-agent completed tasks from `agent_metrics.csv`.
6. `plannerSeconds` and all error counters in `result.json`.

`productiveMovementRatio` is completed tasks' service shortest-path distance divided by all measured agent movement. It is a system-level utilization measure; it does not replace throughput or latency. `activeTasksAtEnd` includes both assigned and unassigned tasks; use `unassignedBacklogAtEnd` for queue pressure.
