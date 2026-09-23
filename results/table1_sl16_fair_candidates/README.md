# SL16 candidate coverage experiment

All runs use Sortation Large with 16,000 agents, 250 timesteps, a 1-second
per-timestep limit, `scheduleModel=6`, GreedyHeap distance weight 5, full
assignment, and coarse future-flow (20x10 grid, weight 1). The planner and
simulation settings are identical. Runs were serial on the same host.

| Candidate construction | Finished tasks | Reveal to assign | Open to finish |
| --- | ---: | ---: | ---: |
| Full exact scan (baseline) | 6,673 | 36.66 | 95.99 |
| Nearby Manhattan shortlist, 8 tasks | 5,755 | 4.20 | 105.24 |
| Nearby shortlist, then exact scan below 256 free agents | 5,936 | 4.28 | 105.81 |
| Landmark lower-bound distance, then exact scan | 5,894 | 4.25 | 106.04 |
| Landmark distance and adjacent-zone search, then exact scan | 6,123 | 3.27 | 107.31 |

At timestep 0, the baseline gave candidates to only one of 16,000 free agents,
while the fair modes covered all four agent-ID quartiles. Coverage substantially
reduces the assignment delay, but every tested approximate shortlist lowers
throughput because the resulting task service stage is slower. The fair mode is
experimental and disabled by default (`heapFairCandidateK=0`). No 1,000-step
run is warranted based on these 250-step results.

The matching 100-step baseline and fair runs, along with per-timestep coverage
logs, are in `../table1_sl16_candidate_coverage/`.
