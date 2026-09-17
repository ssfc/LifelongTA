# Development Screen Summary

All 52 development-screen runs used 250 time steps, the released LifelongTA
instances, a 1,000 ms per-step planning budget, and the standard planner. They
reported zero planner errors, scheduler errors, and entry timeouts.

The selection score is the mean, over the four development instances, of a
configuration's completed-task count divided by the highest count for that
scheduler on that instance. This avoids pooling absolute throughput across the
two map families.

| Scheduler | Selected configuration | Mean normalized score |
| --- | --- | ---: |
| TaskMatcher | distance weight 10; Top-K 100; reassignment enabled | 0.99318 |
| CappedHungarian | distance weight 5; task-length weight 0.5; caps 256/512 | 0.97753 |

TaskMatcher's selected configuration was also the highest observed setting on
both Random development points. The Warehouse Small pointwise winners used
distance weight 2, so the 1,000-step confirmation should be interpreted as a
single robust configuration check rather than evidence of a map-independent
optimum.
