# Portable Assignment Development Protocol

The portable schedulers are evaluated through the released LifelongTA driver.
They do not modify instances, task-release rates, task streams, planners, or
per-step planning budgets. Only `scheduleModel` and scheduler parameters vary.

Development instances are Warehouse Small with 200 and 400 agents and Random
with 400 and 800 agents. The initial screen uses 250 time steps to eliminate
clearly weak configurations. The selected configuration for each scheduler is
then rerun for 1,000 time steps on every development instance. Validation uses
the remaining Table 1 agent counts and does not change parameters.

TaskMatcher searches distance weights {2, 5, 10} and Top-K values {25, 50,
100}; it keeps the contest defaults for unopened-task reassignment, old-pair
bias (6), and pickup protection distance (10). CappedHungarian searches
distance weights {1, 5} and task-length weights {0.5, 1}, with its published
candidate caps of 256 agents and 512 tasks.

The 1,000-step confirmation selected TaskMatcher for validation with distance
weight 10, Top-K 100, and reassignment enabled. Its validation points are
Warehouse Small at 300, 500, and 600 agents and Random at 1,200, 1,600, and
2,000 agents. CappedHungarian is retained as a capped small-scale baseline and
is not extended to these high-density points.

For the released LifelongTA Table 2 finite-workload streams, TaskMatcher uses
the same fixed validation configuration without further tuning: distance
weight 10, Top-K 100, reassignment enabled, old-pair bias 6, and pickup
protection distance 10. It runs the published $3 \times 3$ release/agent grid
for all 25 task-stream seeds per cell, with the original 400-step cap.
