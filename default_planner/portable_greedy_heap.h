#pragma once

#include "SharedEnv.h"
#include "Types.h"

#include <chrono>
#include <unordered_map>
#include <utility>
#include <vector>

namespace DefaultPlanner {

struct PortableGreedyHeapConfig {
    float dist_weight = 5.0f;
    float max_assign_ratio = 1.0f;
    bool reassign_enabled = true;
    float reassign_keep_bias = 6.0f;
    int reassign_min_dist = 10;
    int rebuild_pct = 45;
    int lns_pct = 10;
    int sort_k = 500;
    // Large-map mode: keep candidate construction linear in nearby tasks and
    // bias assignments away from corridors already occupied by opened work.
    bool flow_aware_spatial = false;
    int spatial_cell_size = 16;
    int spatial_candidate_limit = 1000;
    float traffic_weight = 1.0f;
    float reassign_min_gain = 0.0f;
    int future_flow_cell_size = 32;
    float future_flow_weight = 0.0f;
    float future_flow_hard_cap = 0.0f;
    bool future_flow_regret_order = true;
    // Coarse congestion inferred from tasks that have already been picked up.
    // This is independent from DefaultPlanner's background_flow and is useful
    // when the open workload itself dominates the traffic pattern.
    int opened_flow_zone_rows = 0;
    int opened_flow_zone_cols = 0;
    float opened_flow_penalty_weight = 0.0f;
};

class PortableGreedyHeapScheduler {
public:
    void schedule(int time_limit_ms, std::vector<int>& proposed_schedule,
                  SharedEnvironment* env, const PortableGreedyHeapConfig& config,
                  const std::vector<Double4>& background_flow);

private:
    struct AgentInfo {
        int id;
        int location;
    };
    struct TaskInfo {
        int id;
        int start_location;
        std::vector<int> locations;
    };
    struct Candidate {
        float score;
        int task_index;
    };

    using Match = std::pair<int, int>;

    std::vector<std::vector<Candidate>> candidates_;
    std::vector<int> candidate_agent_ids_;
    std::vector<int> candidate_agent_locations_;
    std::vector<int> candidate_task_ids_;
    std::unordered_map<int, float> opened_task_flow_penalties_;

    float score(SharedEnvironment* env, int agent_location, const TaskInfo& task,
                const PortableGreedyHeapConfig& config,
                const std::vector<Double4>& background_flow) const;
    void build_opened_task_flow_penalties(SharedEnvironment* env,
                                          const std::vector<TaskInfo>& tasks,
                                          const PortableGreedyHeapConfig& config);
    void rebuild_candidates(SharedEnvironment* env, const std::vector<AgentInfo>& agents,
                            const std::vector<TaskInfo>& tasks,
                            const PortableGreedyHeapConfig& config,
                            const std::vector<Double4>& background_flow,
                            int sort_k, std::chrono::steady_clock::time_point deadline);
    std::vector<Match> lazy_match(const std::vector<AgentInfo>& agents,
                                  const std::vector<TaskInfo>& tasks,
                                  std::chrono::steady_clock::time_point deadline) const;
    std::vector<Match> flow_aware_match(SharedEnvironment* env,
                                        const std::vector<AgentInfo>& agents,
                                        const std::vector<TaskInfo>& tasks,
                                        const PortableGreedyHeapConfig& config,
                                        const std::vector<Double4>& background_flow,
                                        std::chrono::steady_clock::time_point deadline) const;
    void refine_lns(SharedEnvironment* env, std::vector<Match>& matches,
                    const std::vector<AgentInfo>& agents,
                    const std::vector<TaskInfo>& tasks,
                    const std::unordered_map<int, int>& old_assignment,
                    const PortableGreedyHeapConfig& config,
                    const std::vector<Double4>& background_flow,
                    std::chrono::steady_clock::time_point deadline) const;
};

void schedule_plan_portable_greedy_heap(int time_limit_ms,
                                        std::vector<int>& proposed_schedule,
                                        SharedEnvironment* env,
                                        const PortableGreedyHeapConfig& config,
                                        const std::vector<Double4>& background_flow = {});

}  // namespace DefaultPlanner
