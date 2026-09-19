# Experiment metrics

Each simulation result also writes four files beside its result JSON:

- `result.task_metrics.csv`: one row for every revealed task.
- `result.agent_metrics.csv`: per-agent movement, waiting, and completions.
- `result.timeline_metrics.csv`: per-timestep backlog, agent state, and planner time.
- `result.metrics_summary.json`: aggregate metrics for method comparisons.

Throughput remains the primary metric. Use the additional files to explain a
throughput difference through task latency, first-assignment pickup distance,
empty movement, loaded detours, active waiting, backlog, and planner time.

Task lifecycle timestamps are measured at reveal, first assignment, first
pickup, and completion. Latency summaries include only completed tasks, so
unfinished work must be read through `activeTasksAtEnd` and
`unassignedBacklogAtEnd`.

The benchmark uses a saturated task pool: completing a task reveals the next
task from the deterministic input sequence. Consequently, methods with higher
throughput may have higher `tasksRevealed`; this is an outcome of the workload
model, not a different random task stream. Do not compare latency aggregates
without reporting throughput and end-of-run backlog alongside them.
