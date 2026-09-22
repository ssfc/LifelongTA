#pragma once

#include "SharedEnv.h"

#include <array>
#include <chrono>
#include <unordered_map>
#include <utility>
#include <vector>

namespace DefaultPlanner {

struct PortableGreedyHeapConfig {
    float dist_weight = 5.0f;
    float max_assign_ratio = 1.0f;
    int warmup_steps = 0;
    float warmup_assign_ratio = 1.0f;
    bool reassign_enabled = true;
    float reassign_keep_bias = 6.0f;
    int reassign_min_dist = 10;
    int rebuild_pct = 45;
    int lns_pct = 10;
    int sort_k = 500;
    int flow_zone_rows = 0;
    int flow_zone_cols = 0;
    float flow_penalty_weight = 0.0f;
};

class PortableGreedyHeapScheduler {
public:
    void schedule(int time_limit_ms, std::vector<int>& proposed_schedule,
                  SharedEnvironment* env, const PortableGreedyHeapConfig& config);

private:
    struct AgentInfo {
        int id;
        int location;
    };
    struct TaskInfo {
        int id;
        int start_location;
        std::vector<int> locations;
        // The service-leg distance is independent of the candidate agent.
        // Keep it for the current scheduling call instead of recomputing it
        // for every agent-task candidate.
        mutable int cached_remaining_length = -1;
        mutable float cached_flow_penalty = -1.0f;
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
    int flow_zone_rows_ = 0;
    int flow_zone_cols_ = 0;
    float flow_penalty_weight_ = 0.0f;
    std::vector<std::array<float, 4>> opened_zone_flow_;

    float score(SharedEnvironment* env, int agent_location, const TaskInfo& task,
                float dist_weight) const;
    void build_opened_zone_flow(SharedEnvironment* env,
                                const PortableGreedyHeapConfig& config);
    int zone_of_location(SharedEnvironment* env, int location) const;
    void add_zone_path(int from_zone, int to_zone, float weight);
    float zone_path_penalty(int from_zone, int to_zone) const;
    void rebuild_candidates(SharedEnvironment* env, const std::vector<AgentInfo>& agents,
                            const std::vector<TaskInfo>& tasks, float dist_weight,
                            int sort_k,
                            std::chrono::steady_clock::time_point deadline);
    std::vector<Match> lazy_match(const std::vector<AgentInfo>& agents,
                                  const std::vector<TaskInfo>& tasks,
                                  std::chrono::steady_clock::time_point deadline) const;
    void refine_lns(SharedEnvironment* env, std::vector<Match>& matches,
                    const std::vector<AgentInfo>& agents,
                    const std::vector<TaskInfo>& tasks,
                    const std::unordered_map<int, int>& old_assignment,
                    const PortableGreedyHeapConfig& config,
                    std::chrono::steady_clock::time_point deadline) const;
};

void schedule_plan_portable_greedy_heap(int time_limit_ms,
                                        std::vector<int>& proposed_schedule,
                                        SharedEnvironment* env,
                                        const PortableGreedyHeapConfig& config);

}  // namespace DefaultPlanner
