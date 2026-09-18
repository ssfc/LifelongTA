# Free-Reassignment TaskMatcher: Table 2 Screen

This directory evaluates the single TaskMatcher configuration used for the
final Table 1 comparison: pickup-distance weight 10, top-k 100, and free
reassignment of unopened tasks (keep bias 0; protected pickup distance 0).
All runs use the finite 500-task Table 2 workload, a 400-step cap, and the
released one-second planning budget.

The initial screen uses seeds 0--4 and is compared against Flow on the same
five task streams. Lower makespan is better.

| Release / agents | TaskMatcher free | Flow, same seeds | Flow, 25 seeds | Decision |
| --- | ---: | ---: | ---: | --- |
| 2 / 50 | 301.4 | 299.0 | 299.56 | stop |
| 2 / 80 | 239.0 | 240.6 | 236.76 | expand |
| 2 / 100 | 215.0 | 222.2 | 223.12 | expand |
| 5 / 50 | 273.6 | 276.0 | 272.72 | expand |
| 5 / 80 | 199.4 | 196.6 | 197.36 | stop |
| 5 / 100 | 174.8 | 168.6 | 171.08 | stop |
| 10 / 50 | 270.4 | 274.8 | 270.84 | expand |
| 10 / 80 | 192.8 | 190.6 | 191.24 | stop |
| 10 / 100 | 166.6 | 168.0 | 166.12 | expand |

The expansion runs seeds 5--24 only for the five `expand` cells. The screen is
for deciding compute allocation; it is not a formal statistical conclusion.

## 25-seed confirmation

For the expanded cells, the following table reports mean makespan and sample
standard deviation over all 25 paired task streams. `Delta` is TaskMatcher minus
Flow, so a negative value favors TaskMatcher. The interval is the paired 95%
confidence interval for that delta.

| Release / agents | TaskMatcher free | Flow | Delta | Paired 95% CI | Conclusion |
| --- | ---: | ---: | ---: | ---: | --- |
| 2 / 80 | 243.16 +/- 9.78 | 236.76 +/- 7.55 | 6.40 | [2.67, 10.13] | Flow better |
| 2 / 100 | 222.48 +/- 10.12 | 223.12 +/- 10.39 | -0.64 | [-5.20, 3.92] | inconclusive |
| 5 / 50 | 272.36 +/- 6.99 | 272.72 +/- 9.48 | -0.36 | [-4.39, 3.67] | inconclusive |
| 10 / 50 | 269.40 +/- 8.57 | 270.84 +/- 8.43 | -1.44 | [-4.78, 1.90] | inconclusive |
| 10 / 100 | 165.04 +/- 6.04 | 166.12 +/- 6.49 | -1.08 | [-3.49, 1.33] | inconclusive |

All 125 TaskMatcher runs finish the finite workload with zero planner,
scheduler, and entry-timeout errors. None of the candidate cells has a paired
confidence interval wholly below zero, so this experiment does not support a
claim that TaskMatcher outperforms Flow on Table 2.
