# External LifelongTA Table 1: Free-Reassignment TaskMatcher

This is the consolidated Table 1 result set used in the paper's external
LifelongTA comparison. Every row uses the released 1,000-step input and the
released one-second planning limit. Throughput is completed tasks; higher is
better.

TaskMatcher uses pickup-distance weight 10, top-k 100, exact matching whenever
the candidate matrix has at most 2,000,000 elements, and free reassignment of
unopened tasks (old-pair bias 0; protected pickup distance 0). All ten
TaskMatcher JSON outputs have zero planner errors, scheduler errors, and entry
timeouts.

| Map | Agents | Greedy | Flow | GreedyHeap | TaskMatcher | TaskMatcher raw output |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Warehouse Small | 200 | 3545 | 3675 | 3734 | **3789** | `table1_free_completion/warehouse-small_200_free_reassign_t1000.json` |
| Warehouse Small | 300 | 4767 | 4945 | 5006 | **5064** | `table1_free_completion/warehouse-small_300_free_reassign_t1000.json` |
| Warehouse Small | 400 | 5475 | 5726 | 5686 | **5752** | `table1_free_completion/warehouse-small_400_free_reassign_t1000.json` |
| Warehouse Small | 500 | 5778 | 5824 | 5726 | **5883** | `table1_loss_confirm/warehouse-small_500_free_reassign_t1000.json` |
| Warehouse Small | 600 | 5657 | **5765** | 5508 | 5743 | `table1_loss_confirm/warehouse-small_600_free_reassign_t1000.json` |
| Random | 400 | 6929 | 6964 | 6809 | **7034** | `table1_loss_confirm/random_400_free_reassign_t1000.json` |
| Random | 800 | 12648 | 12878 | 12478 | **12948** | `table1_loss_confirm/random_800_free_reassign_t1000.json` |
| Random | 1200 | 15896 | 16403 | 15784 | **16460** | `table1_loss_confirm/random_1200_free_reassign_t1000.json` |
| Random | 1600 | 16419 | 16101 | 16214 | **17531** | `table1_free_completion/random_1600_free_reassign_t1000.json` |
| Random | 2000 | 14989 | 15987 | 15456 | **16174** | `table1_loss_confirm/random_2000_free_reassign_t1000.json` |

TaskMatcher has the highest observed throughput in nine of ten rows. This is a
configuration-development comparison: the free-reassignment rule was selected
after a 250-step diagnostic on the six rows where the initial fixed TaskMatcher
setting trailed Flow. It is not a held-out multi-seed estimate.
