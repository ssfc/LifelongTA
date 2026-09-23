#pragma once

#include "SharedEnv.h"
#include "Types.h"

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
    float age_bonus = 0.0f;
    int local_exchange_top_k = 0;
    int traffic_rerank_top_k = 0;
    int traffic_rerank_steps = 4;
    float traffic_rerank_weight = 0.0f;
    int candidate_diag_every = 0;
    int fair_candidate_k = 0;
    int fair_exact_agent_threshold = 256;
    bool exact_pickup_cache = false;
};

class PortableGreedyHeapScheduler {
public:
    void prepare_fair_landmarks(SharedEnvironment* env);
    void prepare_exact_pickup_cache(SharedEnvironment* env,
                                    const std::vector<int>& pickup_sites);
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
        int revealed_timestep = -1;
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

    struct CandidateBuildStats {
        int full_scans = 0;
        int partial_scans = 0;
        int seeded_agents = 0;
        int exact_agents = 0;
        long long evaluated_pairs = 0;
    };

    std::vector<std::vector<Candidate>> candidates_;
    std::vector<int> candidate_agent_ids_;
    std::vector<int> candidate_agent_locations_;
    std::vector<int> candidate_task_ids_;
    CandidateBuildStats last_build_stats_;
    int flow_zone_rows_ = 0;
    int flow_zone_cols_ = 0;
    float flow_penalty_weight_ = 0.0f;
    float age_bonus_ = 0.0f;
    int current_timestep_ = 0;
    std::vector<std::array<float, 4>> opened_zone_flow_;
    std::vector<std::vector<int>> fair_landmarks_;
    size_t fair_landmark_map_size_ = 0;
    std::vector<std::vector<int>> exact_pickup_distances_;
    std::unordered_map<int, int> exact_pickup_index_;
    bool exact_pickup_cache_enabled_ = false;

    float score(SharedEnvironment* env, int agent_location, const TaskInfo& task,
                float dist_weight) const;
    void build_opened_zone_flow(SharedEnvironment* env,
                                const PortableGreedyHeapConfig& config);
    int zone_of_location(SharedEnvironment* env, int location) const;
    void add_zone_path(int from_zone, int to_zone, float weight);
    float zone_path_penalty(int from_zone, int to_zone) const;
    int fair_distance(SharedEnvironment* env, int first, int second) const;
    int exact_distance(SharedEnvironment* env, int first, int second) const;
    void rebuild_candidates(SharedEnvironment* env, const std::vector<AgentInfo>& agents,
                            const std::vector<TaskInfo>& tasks, float dist_weight,
                            int sort_k,
                            std::chrono::steady_clock::time_point deadline);
    void rebuild_candidates_fair(SharedEnvironment* env, const std::vector<AgentInfo>& agents,
                                 const std::vector<TaskInfo>& tasks,
                                 const PortableGreedyHeapConfig& config,
                                 std::chrono::steady_clock::time_point deadline);
    void rerank_traffic_candidates(SharedEnvironment* env, const std::vector<AgentInfo>& agents,
                                   const std::vector<TaskInfo>& tasks,
                                   const std::vector<Double4>& background_flow,
                                   const PortableGreedyHeapConfig& config,
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
    void refine_local_exchanges(SharedEnvironment* env, std::vector<Match>& matches,
                                const std::vector<AgentInfo>& agents,
                                const std::vector<TaskInfo>& tasks,
                                const PortableGreedyHeapConfig& config,
                                std::chrono::steady_clock::time_point deadline) const;
};

void schedule_plan_portable_greedy_heap(int time_limit_ms,
                                        std::vector<int>& proposed_schedule,
                                        SharedEnvironment* env,
                                        const PortableGreedyHeapConfig& config,
                                        const std::vector<Double4>& background_flow);
void prepare_portable_greedy_heap(SharedEnvironment* env,
                                  const PortableGreedyHeapConfig& config,
                                  const std::vector<int>& pickup_sites);

}  // namespace DefaultPlanner
