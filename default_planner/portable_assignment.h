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
    bool use_traffic_cost = false;
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
                                          const std::vector<Double4>& background_flow);

void schedule_plan_portable_capped_hungarian(
    int time_limit_ms, std::vector<int>& proposed_schedule, SharedEnvironment* env,
    const PortableCappedHungarianConfig& config);

}  // namespace DefaultPlanner
