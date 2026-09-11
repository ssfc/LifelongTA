#include "portable_greedy_heap.h"

#include "heuristics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <queue>
#include <random>
#include <unordered_set>

namespace DefaultPlanner {
namespace {

int remaining_task_length(SharedEnvironment* env, const Task& task)
{
    if (task.idx_next_loc < 0 || task.idx_next_loc >= static_cast<int>(task.locations.size())) {
        return 0;
    }
    int length = 0;
    for (size_t i = static_cast<size_t>(task.idx_next_loc + 1); i < task.locations.size(); ++i) {
        length += get_h(env, task.locations[i - 1], task.locations[i]);
    }
    return length;
}

}  // namespace

float PortableGreedyHeapScheduler::score(SharedEnvironment* env, int agent_location,
                                         const TaskInfo& task, float dist_weight) const
{
    if (task.locations.empty()) return std::numeric_limits<float>::infinity();
    int distance = get_h(env, agent_location, task.start_location);
    int length = 0;
    for (size_t i = 1; i < task.locations.size(); ++i) {
        length += get_h(env, task.locations[i - 1], task.locations[i]);
    }
    return dist_weight * static_cast<float>(distance) + static_cast<float>(length);
}

void PortableGreedyHeapScheduler::rebuild_candidates(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, float dist_weight, int sort_k,
    std::chrono::steady_clock::time_point deadline)
{
    std::vector<int> task_ids;
    task_ids.reserve(tasks.size());
    for (const TaskInfo& task : tasks) task_ids.push_back(task.id);
    const bool same_task_set = candidate_task_ids_ == task_ids;
    const bool same_shape = candidates_.size() == agents.size() &&
                            candidate_agent_ids_.size() == agents.size() &&
                            candidate_agent_locations_.size() == agents.size();

    if (!same_shape || !same_task_set) {
        candidates_.assign(agents.size(), {});
        candidate_agent_ids_.assign(agents.size(), -1);
        candidate_agent_locations_.assign(agents.size(), -1);
        candidate_task_ids_ = std::move(task_ids);
    }

    for (size_t ai = 0; ai < agents.size(); ++ai) {
        if (std::chrono::steady_clock::now() >= deadline) break;
        if (same_task_set && same_shape && candidate_agent_ids_[ai] == agents[ai].id &&
            candidate_agent_locations_[ai] == agents[ai].location && !candidates_[ai].empty()) {
            continue;
        }
        auto& list = candidates_[ai];
        list.clear();
        list.reserve(tasks.size());
        for (size_t ti = 0; ti < tasks.size(); ++ti) {
            if ((ti & 31U) == 0U && std::chrono::steady_clock::now() >= deadline) break;
            list.push_back({score(env, agents[ai].location, tasks[ti], dist_weight),
                            static_cast<int>(ti)});
        }
        if (list.empty()) continue;
        const int keep = sort_k > 0 ? std::min(sort_k, static_cast<int>(list.size()))
                                    : static_cast<int>(list.size());
        if (keep < static_cast<int>(list.size())) {
            std::nth_element(list.begin(), list.begin() + keep, list.end(),
                             [](const Candidate& lhs, const Candidate& rhs) {
                                 return lhs.score < rhs.score;
                             });
            list.resize(keep);
        }
        std::sort(list.begin(), list.end(), [](const Candidate& lhs, const Candidate& rhs) {
            return lhs.score < rhs.score;
        });
        candidate_agent_ids_[ai] = agents[ai].id;
        candidate_agent_locations_[ai] = agents[ai].location;
    }
}

std::vector<PortableGreedyHeapScheduler::Match> PortableGreedyHeapScheduler::lazy_match(
    const std::vector<AgentInfo>& agents, const std::vector<TaskInfo>& tasks,
    std::chrono::steady_clock::time_point deadline) const
{
    struct HeapItem {
        float score;
        int agent_index;
        int candidate_index;
        bool operator>(const HeapItem& other) const { return score > other.score; }
    };

    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
    for (int ai = 0; ai < static_cast<int>(candidates_.size()); ++ai) {
        if (!candidates_[ai].empty()) heap.push({candidates_[ai][0].score, ai, 0});
    }

    std::vector<bool> used_agents(agents.size(), false);
    std::vector<bool> used_tasks(tasks.size(), false);
    std::vector<Match> matches;
    while (!heap.empty() && std::chrono::steady_clock::now() < deadline) {
        const HeapItem item = heap.top();
        heap.pop();
        if (item.agent_index < 0 || item.agent_index >= static_cast<int>(agents.size()) ||
            used_agents[item.agent_index]) {
            continue;
        }
        const auto& list = candidates_[item.agent_index];
        if (item.candidate_index < 0 || item.candidate_index >= static_cast<int>(list.size())) {
            continue;
        }
        const int task_index = list[item.candidate_index].task_index;
        if (task_index >= 0 && task_index < static_cast<int>(tasks.size()) && !used_tasks[task_index]) {
            used_agents[item.agent_index] = true;
            used_tasks[task_index] = true;
            matches.emplace_back(agents[item.agent_index].id, tasks[task_index].id);
            continue;
        }
        const int next = item.candidate_index + 1;
        if (next < static_cast<int>(list.size())) {
            heap.push({list[next].score, item.agent_index, next});
        }
    }
    return matches;
}

void PortableGreedyHeapScheduler::refine_lns(
    SharedEnvironment* env, std::vector<Match>& matches, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const std::unordered_map<int, int>& old_assignment,
    const PortableGreedyHeapConfig& config, std::chrono::steady_clock::time_point deadline) const
{
    if (matches.size() < 2) return;
    std::unordered_map<int, int> location_by_agent;
    std::unordered_map<int, const TaskInfo*> task_by_id;
    for (const AgentInfo& agent : agents) location_by_agent[agent.id] = agent.location;
    for (const TaskInfo& task : tasks) task_by_id[task.id] = &task;

    auto pair_score = [&](int agent_id, int task_id) {
        const auto agent_it = location_by_agent.find(agent_id);
        const auto task_it = task_by_id.find(task_id);
        if (agent_it == location_by_agent.end() || task_it == task_by_id.end()) {
            return std::numeric_limits<float>::infinity();
        }
        float value = score(env, agent_it->second, *task_it->second, config.dist_weight);
        const auto old_it = old_assignment.find(agent_id);
        if (old_it != old_assignment.end() && old_it->second == task_id) {
            value -= config.reassign_keep_bias;
        }
        return value;
    };

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> choose(0, static_cast<int>(matches.size()) - 1);
    while (std::chrono::steady_clock::now() < deadline) {
        const int first = choose(rng);
        const int second = choose(rng);
        if (first == second) continue;
        const Match& a = matches[first];
        const Match& b = matches[second];
        const float before = pair_score(a.first, a.second) + pair_score(b.first, b.second);
        const float after = pair_score(a.first, b.second) + pair_score(b.first, a.second);
        if (after < before) std::swap(matches[first].second, matches[second].second);
    }
}

void PortableGreedyHeapScheduler::schedule(int time_limit_ms, std::vector<int>& proposed_schedule,
                                           SharedEnvironment* env,
                                           const PortableGreedyHeapConfig& config)
{
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds(std::max(0, time_limit_ms));
    const int agent_count = env->num_of_agents;
    proposed_schedule.assign(agent_count, -1);

    std::vector<int> local(agent_count, -1);
    for (int agent = 0; agent < agent_count && agent < static_cast<int>(env->curr_task_schedule.size()); ++agent) {
        local[agent] = env->curr_task_schedule[agent];
    }

    std::unordered_map<int, int> owner_by_task;
    for (int agent = 0; agent < agent_count; ++agent) {
        if (local[agent] >= 0) owner_by_task[local[agent]] = agent;
    }

    std::vector<AgentInfo> free_agents;
    std::vector<AgentInfo> reassign_agents;
    std::vector<TaskInfo> free_tasks;
    std::vector<TaskInfo> reassign_tasks;
    std::unordered_map<int, int> old_assignment;
    std::unordered_set<int> locked_tasks;

    for (int agent = 0; agent < agent_count; ++agent) {
        const int task_id = local[agent];
        if (task_id < 0) {
            free_agents.push_back({agent, env->curr_states.at(agent).location});
            continue;
        }
        const auto task_it = env->task_pool.find(task_id);
        if (task_it == env->task_pool.end()) {
            local[agent] = -1;
            free_agents.push_back({agent, env->curr_states.at(agent).location});
            continue;
        }
        const Task& task = task_it->second;
        if (task.idx_next_loc > 0 || !config.reassign_enabled) {
            locked_tasks.insert(task_id);
            continue;
        }
        const int pickup = task.locations.at(task.idx_next_loc);
        const int distance = get_h(env, env->curr_states.at(agent).location, pickup);
        if (config.reassign_min_dist > 0 && distance < config.reassign_min_dist) {
            locked_tasks.insert(task_id);
            continue;
        }
        reassign_agents.push_back({agent, env->curr_states.at(agent).location});
        reassign_tasks.push_back({task_id, pickup,
                                  std::vector<int>(task.locations.begin() + task.idx_next_loc,
                                                   task.locations.end())});
        old_assignment[agent] = task_id;
    }

    for (const auto& entry : env->task_pool) {
        const int task_id = entry.first;
        const Task& task = entry.second;
        if (task.idx_next_loc < 0 || task.idx_next_loc >= static_cast<int>(task.locations.size())) continue;
        if (locked_tasks.count(task_id) || owner_by_task.count(task_id)) continue;
        free_tasks.push_back({task_id, task.locations.at(task.idx_next_loc),
                              std::vector<int>(task.locations.begin() + task.idx_next_loc,
                                               task.locations.end())});
    }

    const int rebuild_ms = std::max(0, time_limit_ms) * std::clamp(config.rebuild_pct, 0, 100) / 100;
    const int lns_ms = std::max(0, time_limit_ms) * std::clamp(config.lns_pct, 0, 100) / 100;
    const auto rebuild_deadline = std::min(deadline, start + std::chrono::milliseconds(rebuild_ms));
    const auto lns_deadline = deadline - std::chrono::milliseconds(std::min(lns_ms, std::max(0, time_limit_ms)));

    rebuild_candidates(env, free_agents, free_tasks, config.dist_weight, config.sort_k, rebuild_deadline);
    std::vector<Match> matches = lazy_match(free_agents, free_tasks, lns_deadline);

    std::vector<AgentInfo> refinement_agents = free_agents;
    refinement_agents.insert(refinement_agents.end(), reassign_agents.begin(), reassign_agents.end());
    std::vector<TaskInfo> refinement_tasks = free_tasks;
    refinement_tasks.insert(refinement_tasks.end(), reassign_tasks.begin(), reassign_tasks.end());
    for (const AgentInfo& agent : reassign_agents) {
        matches.emplace_back(agent.id, old_assignment.at(agent.id));
    }
    refine_lns(env, matches, refinement_agents, refinement_tasks, old_assignment, config, deadline);

    std::unordered_set<int> matched_agents;
    std::unordered_set<int> matched_tasks;
    for (const Match& match : matches) {
        if (matched_agents.insert(match.first).second && matched_tasks.insert(match.second).second) {
            local[match.first] = match.second;
        }
    }
    for (const AgentInfo& agent : free_agents) {
        if (matched_agents.count(agent.id) == 0) local[agent.id] = -1;
    }
    proposed_schedule = std::move(local);
}

void schedule_plan_portable_greedy_heap(int time_limit_ms, std::vector<int>& proposed_schedule,
                                        SharedEnvironment* env,
                                        const PortableGreedyHeapConfig& config)
{
    static PortableGreedyHeapScheduler scheduler;
    scheduler.schedule(time_limit_ms, proposed_schedule, env, config);
}

}  // namespace DefaultPlanner
