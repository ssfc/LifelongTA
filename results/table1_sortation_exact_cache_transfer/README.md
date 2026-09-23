# Exact pickup-distance cache across Sortation Large sizes

The experimental switch `--heapExactPickupCache true` precomputes exact
shortest-path distances from the 620 pickup sites marked `E` on the public
Sortation Large map. It does not read unrevealed tasks. All runs use the same
released inputs, DefaultPlanner, 1-second per-timestep budget, 30-second
preprocessing budget, and 1,000-timestep horizon. Runs were serial on the
same host. See `scripts/run_table1_sortation_exact_cache_transfer.sh`.

| Agents | Uncached GreedyHeap | Exact cache | Difference | Reference |
| ---: | ---: | ---: | ---: | --- |
| 4,000 | 15,200 | 15,324 | +124 (+0.8%) | `../table1_ours_sortation_completion/` |
| 8,000 | 28,137 | 28,712 | +575 (+2.0%) | `../table1_ours_sortation_completion/` |
| 12,000 | 36,935 | 38,272 | +1,337 (+3.6%) | `../table1_ours_sortation_completion/` |
| 16,000 | 35,355 +/- 91 | 36,606 +/- 316 | +1,251 (+3.5%) | `../table1_sl16_local_flow_paired/`; cached results in `../table1_sl16_exact_shortlist/` |
| 20,000 | 33,332 | 34,139 +/- 213 | +807 (+2.4%) | `../table1_sortation_future_flow_followup/` |

Values are completed tasks. The 16,000-agent and cached 20,000-agent entries
are means +/- sample standard deviations over three runs; all other entries
are single runs. The 20,000-agent pair both uses 60% assignment and coarse
future-flow penalty (20x10 zones, weight 1); the 4,000/8,000/12,000-agent
pairs use full assignment without future-flow. The cached 16,000-agent pair
uses full assignment with coarse future-flow. Thus rows should be compared
within each size, not as one common scheduler configuration.

The 20,000-agent cached runs finished 34,166, 33,914, and 34,337 tasks. The
second had one entry timeout; all other cached runs listed here had zero
entry timeouts. Every cached run had zero planner and scheduler errors. The
local single-run Flow-Traffic reference at 20,000 agents finished 32,414
tasks (`../table1_sortation_flow_current/`). The 20,000-agent Flow and
uncached GreedyHeap references need matched repeats before a statistical
comparison. The one-run 4,000/8,000/12,000-agent checks establish no observed
regression, but small differences should not be treated as stable gains.

The 250-timestep 20,000-agent screen finished 9,098 tasks with the new cache
versus 7,336 for the prior same-configuration future-flow GreedyHeap. The
cache is disabled by default; no spatial shortlist was used in these runs.
