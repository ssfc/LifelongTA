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
    bool use_traffic_cost = false;
    int traffic_top_k = 50;
    float traffic_congestion_weight = 1.0F;
    // Adds a low-cost congestion proxy for pickup-to-delivery service legs.
    // Zero preserves the original traffic matcher, which only scores the
    // agent-to-pickup path with traffic.
    float traffic_service_weight = 0.0F;
    // Use a bounded spatial matcher instead of the O(agents * tasks) large
    // instance fallback.  Disabled by default to preserve existing results.
    bool scalable_mode = false;
    int scalable_bucket_size = 8;
    int scalable_candidates_per_bucket = 2;
    // Preserve global competition at large scale without building the full
    // agent-by-task matrix.  Each agent contributes a bounded spatially
    // retrieved candidate list to one lazy global min-heap.
    bool scalable_global_heap = false;
    int scalable_global_candidates = 24;
    // Values below one reserve the remaining agents/tasks for the bounded
    // spatial fallback, so sparse global candidates cannot reduce coverage.
    float scalable_global_heap_fraction = 1.0F;
    // HardWarehouse-inspired sparse pair graph.  Candidate edges are drawn
    // from both the agent and task side, constrained by a zone radius and
    // connected components, before the global heap is formed.
    bool scalable_bilateral_heap = false;
    int scalable_task_candidates = 8;
    int scalable_zone_radius = 4;
    // Softly distribute one scheduling round across pickup zones while
    // retaining the coverage-preserving spatial matcher.
    bool scalable_zone_capacity = false;
    int scalable_zone_capacity_limit = 8;
    float scalable_zone_capacity_weight = 20.0F;
    int scalable_zone_capacity_radius = 1;
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
