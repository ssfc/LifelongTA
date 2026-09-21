# Table 2：尾部瓶颈匹配 V1（dormitory）

## 实施范围

实现位于 `C:\Users\34288\.codex\worktrees\table2-diagnose\LifelongTA`，这是此前有限 500-task Table 2 campaign 的源码工作树；不是主目录的 lifelong 循环任务版本。原有未提交修改全部保留。

- 新核心：`default_planner/tail_bottleneck.h`，整数、确定性、独立于 planner 的阈值匹配与受限 Hungarian。
- 调度接线：`default_planner/portable_assignment.cpp/.h`、`src/TaskScheduler.cpp`、`src/driver.cpp`。
- 开关：`--matcherTailBottleneck true`，默认 false。
- 独立构建：`build-tail-bottleneck/lifelong.exe`，不覆盖历史 `build-win/lifelong.exe`。

## 算法

只在**有限任务源已经释放完、全部可变未 pickup 任务能同时由可调度 agents 覆盖**时启用；状态由 TaskManager 发布 `task_source_exhausted`，不向算法泄漏未揭示任务位置。

先完整计算原 TaskMatcher 分配作为回退。以 agent 到 pickup 的最短距离加任务内部剩余最短距离作为预计完成耗时，最小化以下字典序目标：

1. `max(不可改派任务的剩余时间, 所有新匹配任务的预计完成时间)`；
2. 所有可变 agent 的取货总距离。

阈值二分+增广匹配找最小可行最大时间，再在可行边内用 Hungarian 最小化取货距离。固定载货/受保护分配不变。只有完整候选严格改善目标才替换原分配；等目标、无解、非法输入或截止时间到均保留原方案。距离只是代理预测，不保证实际 PIBT 完成时间改善。

每次符合触发条件会输出 `tailBottleneckDecisions`：timestep、任务数/agent 数、accepted、deadline_fallback、fixed/候选/基线 tail、pickup 总距离与耗时。这样可以区分未触发、预测未改善、实际改善和预测失效。

## 指标修正

旧 `makespan` 原样保留，并添加明确别名 `maxAgentActiveSteps`。它是最大 agent 累计有任务步数，**不是**一般意义的最后任务完成时刻。

- `actualMakespan`：有限任务全部完成时的最终时刻；未完成为 null。
- `simulationSteps`、`allTasksCompleted`、`numTasksTotal` 独立输出。
- `metrics_summary` 格式升级为 v2：第一步按实际生效分配归因，首次 pickup 距离使用移动前位置；跨取消间隔真正换 agent 才计重分配。仿真 pickup/completion 的原检查顺序不变。
- 原 first-hop 指标仍是静态最短路代理，不是实际 PIBT 动作。

共同 planner/search/pibt 未修改，包括已发现的历史未初始化读取和墙钟敏感性。这是新算法初筛，不是确定性证明；后续修复共同 planner 时，两方必须使用同一修正版重新比较。逆向流方向问题不在本实验路径，三个方法都禁用 matcher traffic。

## PIBT 预演：未实施

真实 planner 使用共享 `rand()`、全局可变缓存及不可安全浅拷贝的 MemoryPool。不能把静态模拟伪称真实 PIBT 预演。隔离工程合同见该源码工作树的 `docs/planner_rollout_isolation_requirements.md`。V1 不含预演；后续需独立 PlannerContext/RNG/缓存所有权改造和等价性测试。

## 验证和实验

- 独立 solver 测试：`tests/tail_bottleneck_test.cpp`，1600 个小矩阵逐一对照全枚举，含矩形、不可达、fixed-tail、常数项反例、等目标及所有取消检查点。
- 任务统计测试：`tests/task_metrics_test.cpp`，首步、移动前距离、取消再接、无效分配、超时步守恒与原 pickup 顺序。
- 主目录 `scripts/test_table2_tail_bottleneck_windows.ps1`：完整可执行程序微型测试，终端、尚有未来任务、截断结果；测试中预设终端例由 11 步降至 8 步。这是功能测试，不能当作 Table 2 超过 Flow 的证据。
- 主目录 `scripts/run_table2_tail_bottleneck_screen_windows.ps1`：九行 `f=2/5/10 × n=50/80/100`，默认 seed 0，每行 Flow-Unit、TaskMatcher-free、TaskMatcher-tail 三组，共 27 次，交替串行。每次有限 500-task、400-step、共同 1s 预算。
- 输出 `results/dormitory/table2_tail_bottleneck_row_screen`；逐次更新 summary/status。源码、输入 SHA256、精确参数、runner 与可执行文件归档，三方法使用同一可执行文件。不混用旧成绩。存在错误或未完成会停止，不悄悄重跑/覆盖。

判定首先看真实完成时刻，其次看原兼容指标、最后十任务完成时刻、取货距离、空载/载货等待、重分配与规划耗时。只有跨行表现和解释一致后才考虑增加 seed；如果只降低 active-step 指标而实际更晚完成，不视为成功。

## 构建配置

使用 MSYS2 UCRT GCC 15.2、Release、MinGW Makefiles、`PYTHON=OFF`、`Boost_USE_STATIC_LIBS=OFF` 和 `CMAKE_PREFIX_PATH=C:/gitcloud/deps/lemon-ucrt`。Boost 必须动态链接，与原工程的 `BOOST_LOG_DYN_LINK` 一致。
