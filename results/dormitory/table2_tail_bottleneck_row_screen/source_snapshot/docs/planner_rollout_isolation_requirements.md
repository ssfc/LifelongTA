# Requirements for isolated Guided PIBT candidate evaluation

Status: design contract, not an implemented simulator. The current tail-assignment
experiment does not enable planner rollout and does not change the shared planner.

## Actual state and ownership

- `default_planner/planner.cpp` stores live decisions, priorities `p`/`p_copy`,
  idle parking goals, agent order, guide update flags, and `trajLNS` in namespace
  globals. Goal changes reset priority; a task-free agent targets its initial
  location. A snapshot must include all agents, including loaded and idle agents.
- `default_planner/TrajLNS.h` stores references to heuristic tables and neighbors
  (lines 64 and 69 in the inspected version), an environment pointer (line 53),
  and a `MemoryPool` (line 76). Default copying does not provide isolation.
- `default_planner/Memory.h` owns a raw `s_node*` allocation and releases it in its
  destructor. A shallow copy can alias mutable search memory and double-free it.
  Search nodes also hold parent pointers; copying the vector alone is insufficient.
- `default_planner/heuristics.cpp::get_heuristic` and
  `get_source_2_path` advance lazy BFS queues and update distance tables. A query
  is therefore not a read-only operation unless its cache has been materialized
  or is owned by the candidate context.
- `default_planner/pibt.cpp` and `flow.cpp`, and the comparisons in
  `search_node.h`, consume global C `rand()`. There is no portable C/C++ API for
  capturing and restoring that generator's state. Calling the live functions
  during scoring would affect subsequent real decisions.

## Dynamics that must be represented

A candidate evaluation must copy positions, current assignments, known task
progress, priorities, initial parking goals, guides, guide-distance caches,
flow, guide-update ordering, and random state. It must apply the same pickup,
delivery, and idle transitions as the real executor.

Priority resets follow changes in the next goal location, not merely task ID
changes. Pickup-to-delivery transitions also change the goal. Agents with no
task still travel toward their original parking locations and participate in
PIBT displacement. Omitting these agents produces a different simulation.

`src/Entry.cpp` passes scheduling decisions to the planner before actions are
computed. A candidate snapshot must be captured at that decision boundary and
must not include future unrevealed tasks. If the task source is not exhausted,
the rollout must explicitly use only currently known tasks; it cannot claim to
predict the exact future online workload.

## Smallest trustworthy future interface

An explicit owned `PlannerContext` should contain mutable planner state and a
copyable random generator. Planner search, guide updates, and PIBT must consume
that context instead of namespace globals. Immutable map topology can be shared;
mutable caches must be local or use a demonstrably immutable representation.

The intended interface is an immutable snapshot plus candidate evaluation:

```cpp
PlannerSnapshot snapshot(const PlannerContext& live);
RolloutResult evaluate(const PlannerSnapshot& initial,
                       const Assignment& candidate,
                       const RolloutBudget& budget);
```

The horizon should initially be 4--8 steps. Each candidate starts with the same
snapshot and random state. `RolloutResult` must distinguish complete, timed-out,
and invalid evaluations; an incomplete evaluation is not a usable score.

Migrating the real planner to an explicit random generator changes its tie
behavior unless the original sequence is exactly preserved. That migration and
shared planner bug fixes require a new common version for both baseline and
candidate, rather than comparison against old baseline results. The default
planner remains untouched in the current experiment.

## Budget and comparability

The real planner spends remaining wall-clock time after scheduling on guide
optimization (`src/MAPFPlanner.cpp::plan`,
`default_planner/flow.cpp::frank_wolfe`). An expensive candidate scorer therefore
reduces the guide work available to execution. This cost is part of the complete
algorithm and must be reported.

Candidate mechanism tests should use the same deterministic guide-update work
quota. Equal wall-clock limits alone permit different iteration counts and
random-number consumption. Formal runtime evaluation must use the same total
entry budget for all methods.

An outer deadline check does not create a strict budget: the current A* loop has
no internal interruption check, copying large caches may be expensive, and a
PIBT recursion is not currently cancellable. The future implementation must
bound snapshot creation, each search, and recursive execution. Cancellation must
only discard a candidate context, never leave the live context partly mutated.

## Acceptance tests

1. Fingerprint all live state, mutable caches, and RNG state before and after
   evaluation. They must remain identical, including on timeout and failure.
2. A one-step rollout of the incumbent assignment must reproduce the actual
   execution from the same snapshot and deterministic guide-update quota.
3. Verify goal switching and priority behavior for pickup, delivery, task
   reassignment, task ID changes at the same pickup, and idle return-to-parking.
4. Include occupied next cells, recursive displacement chains, opposite-edge
   conflicts, loaded agents, and idle agents in transition tests.
5. Every candidate uses the same initial random state; evaluating a different
   candidate first must not change another candidate's score.
6. Budget interruption must return `incomplete` without publishing assignments
   or consuming live randomness. The caller must retain its completed fallback.
7. Instrument elapsed scoring time and guide work left for execution; compare
   complete algorithms under one shared entry budget.

Until these contracts are met, a frozen-guide or static-route approximation may
be useful as a separately named proxy, but must not be described as a clone of
the real Guided PIBT execution.
