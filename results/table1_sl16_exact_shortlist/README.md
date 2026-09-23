# Sortation Large 16k: exact pickup-distance cache

All runs use the released Sortation Large 16,000-agent instance, 1,000
timesteps, a 1-second per-timestep budget, and the same DefaultPlanner and
GreedyHeap configuration as `../table1_sl16_local_flow_paired/`. Runs were
serial on the same host. The only change to the final GreedyHeap variant is
`--heapExactPickupCache true`; spatial shortlisting is off
(`--heapFairCandidateK 0`). The cache is built from the 620 pickup sites marked
`E` in the public map. It never reads unrevealed tasks.

| Method | Rep 1 | Rep 2 | Rep 3 | Mean +/- sample SD |
| --- | ---: | ---: | ---: | ---: |
| GreedyHeap, exact pickup cache | 36,566 | 36,940 | 36,312 | 36,606 +/- 316 |
| Flow-Traffic, local reference | 35,813 | 36,190 | 34,956 | 35,653 +/- 632 |
| GreedyHeap, uncached reference | 35,250 | 35,395 | 35,419 | 35,355 +/- 91 |

The cached GreedyHeap mean is 953 tasks (2.7%) above local Flow-Traffic and
1,251 tasks (3.5%) above uncached GreedyHeap. All three cached runs have zero
entry timeouts, planner errors, and scheduler errors. Three runs are too few
for a strong statistical claim. The local Flow-Avg Waiting reference is one
run at 35,441 tasks (`../table1_sortation_flow_current/`).

At timestep 0, the cached full scan built complete task lists for 377 agents
and a partial list for one more, versus one partial list in the uncached
baseline. A 250-timestep paired screen completed 7,320 tasks with the cache,
6,673 without it, and 5,880 with both the cache and the experimental spatial
shortlist (`--heapFairCandidateK 8`). The shortlist is a negative result: its
restricted task coverage outweighed the faster initial assignment. It is
disabled in the final configuration.

Reproduce the cached runs with
`bash scripts/run_table1_sl16_exact_shortlist.sh 1000 0 <replicate>` from the
repository root, with replicate values 1, 2, and 3. The script writes JSON
and logs in this directory.
