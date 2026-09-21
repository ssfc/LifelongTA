#pragma once

#include "SharedEnv.h"
#include "Types.h"

#include <vector>

namespace DefaultPlanner {

// Portable counterparts of the contest TaskMatcher and CappedHungarian
// schedulers. They use only the standard LifelongTA scheduler interface.
struct PortableTaskMatcherConfig {
    float dist_weight = 5.0f;
    float task_length_weight = 1.0f;
    bool reassign_enabled = true;
    float reassign_keep_bias = 6.0f;
    int reassign_min_dist = 10;
    int candidate_top_k = 50;
    int max_matrix_elements = 2000000;
    bool use_traffic_cost = false;
    int traffic_top_k = 50;
    float traffic_congestion_weight = 1.0f;
    // Extra penalty for only the direction opposite to an already opened
    // guide path.  Zero preserves the existing traffic cost exactly.
    float traffic_contraflow_weight = 0.0f;
    bool use_projected_load = false;
    int projected_top_k = 100;
    float projected_load_weight = 0.25f;
    int projected_iterations = 2;
    // Kiva-oriented future-flow model. Unlike projected-load overlap, it
    // penalises only marginal load above a per-cell capacity and includes
    // routes of agents whose assignments are already fixed.
    bool use_bottleneck_flow = false;
    int bottleneck_capacity = 2;
    float bottleneck_penalty_weight = 0.25f;
    int bottleneck_iterations = 2;
    float flow_seed_bias = 0.0f;
    int flow_seed_budget_ms = 100;
    // Once a task has waited past this threshold, reduce its matching cost by
    // age_weight * (age - threshold). This protects the completion-time tail
    // without changing the early, proximity-driven matching behaviour.
    int old_task_age_threshold = 0;
    float old_task_age_weight = 0.0f;
    // After the finite task source is exhausted and all unopened tasks fit
    // onto flexible agents, lexicographically minimize completion tail then
    // pickup distance. Disabled preserves the existing scheduling algorithm.
    bool tail_bottleneck = false;
};

// Per-call diagnostics, reset by schedule_plan_portable_task_matcher. Tail and
// pickup values are shortest-path predictions, NOT observed makespan values.
struct PortableTailBottleneckStats {
    bool eligible = false;
    bool attempted = false;
    bool accepted = false;
    bool deadline_fallback = false;
    bool infeasible = false;
    bool invalid_input = false;
    bool baseline_complete = false;
    int task_count = 0;
    int agent_count = 0;
    long long baseline_tail = -1;
    long long candidate_tail = -1;
    long long locked_tail = -1;
    long long baseline_flexible_tail = -1;
    long long candidate_flexible_tail = -1;
    long long baseline_pickup = -1;
    long long candidate_pickup = -1;
    long long elapsed_us = 0;
};

struct PortableCappedHungarianConfig {
    int max_agents = 256;
    int max_tasks = 512;
    float dist_weight = 1.0f;
    float task_length_weight = 1.0f;
};

struct PortableStableSnatchHungarianConfig {
    int max_agents = 256;
    int max_tasks = 512;
    float dist_weight = 1.0f;
    float task_length_weight = 1.0f;
    int snatch_min_pickup_distance = 10;
    float snatch_min_abs_improve = 6.0f;
    float snatch_min_rel_improve = 0.10f;
};

void schedule_plan_portable_task_matcher(int time_limit_ms,
                                         std::vector<int>& proposed_schedule,
                                         SharedEnvironment* env,
                                         const PortableTaskMatcherConfig& config,
                                         const std::vector<Double4>& background_flow,
                                         PortableTailBottleneckStats* tail_stats = nullptr);

void schedule_plan_portable_capped_hungarian(
    int time_limit_ms, std::vector<int>& proposed_schedule, SharedEnvironment* env,
    const PortableCappedHungarianConfig& config);

void schedule_plan_portable_stable_snatch_hungarian(
    int time_limit_ms, std::vector<int>& proposed_schedule, SharedEnvironment* env,
    const PortableStableSnatchHungarianConfig& config);

}  // namespace DefaultPlanner
