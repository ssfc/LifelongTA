#pragma once

#include "SharedEnv.h"
#include "Types.h"

#include <vector>

namespace DefaultPlanner {

// Portable counterparts of the contest TaskMatcher and CappedHungarian
// schedulers. They use only the standard LifelongTA scheduler interface.
struct PortableTaskMatcherConfig {
    float dist_weight = 5.0f;
    bool reassign_enabled = true;
    float reassign_keep_bias = 6.0f;
    int reassign_min_dist = 10;
    int candidate_top_k = 50;
    int max_matrix_elements = 2000000;
    float max_assign_ratio = 1.0F;
    bool adaptive_assign_ratio = false;
    float wait_priority_weight = 0.0F;
    int wait_priority_threshold = 0;
    bool use_traffic_cost = false;
    float traffic_pressure_threshold = 0.0F;
    bool use_wait_heat = false;
    float wait_heat_weight = 0.0F;
    float wait_heat_pressure_threshold = 1.0F;
    float guide_regret_weight = 0.0F;
    int guide_regret_cap = 20;
    float tail_rescue_weight = 0.0F;
    int tail_rescue_threshold = 60;
    int tail_rescue_cap = 20;
    int traffic_top_k = 50;
    float traffic_congestion_weight = 1.0F;
};

struct PortableCappedHungarianConfig {
    int max_agents = 256;
    int max_tasks = 512;
    float dist_weight = 1.0f;
    float task_length_weight = 1.0f;
};

void schedule_plan_portable_task_matcher(int time_limit_ms,
                                          std::vector<int>& proposed_schedule,
                                          SharedEnvironment* env,
                                          const PortableTaskMatcherConfig& config,
                                          const std::vector<Double4>& background_flow,
                                          const std::vector<int>& guide_path_remaining = {});

void schedule_plan_portable_capped_hungarian(
    int time_limit_ms, std::vector<int>& proposed_schedule, SharedEnvironment* env,
    const PortableCappedHungarianConfig& config);

}  // namespace DefaultPlanner
