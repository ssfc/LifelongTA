#include "portable_assignment.h"

#include "heuristics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace DefaultPlanner {
namespace {

constexpr float kInvalidCost = 1.0e9F;

struct AgentInfo {
    int id;
    int location;
};

struct TaskInfo {
    int id;
    int pickup;
    std::vector<int> locations;
};

float task_score(SharedEnvironment* env, int agent_location, const TaskInfo& task,
                 float dist_weight, float task_length_weight = 1.0F)
{
    if (task.locations.empty()) return kInvalidCost;
    const int distance = get_h(env, agent_location, task.pickup);
    if (distance >= std::numeric_limits<int>::max() / 8) return kInvalidCost;

    int length = 0;
    for (size_t i = 1; i < task.locations.size(); ++i) {
        const int segment = get_h(env, task.locations[i - 1], task.locations[i]);
        if (segment >= std::numeric_limits<int>::max() / 8) return kInvalidCost;
        length += segment;
    }
    return dist_weight * static_cast<float>(distance) +
           task_length_weight * static_cast<float>(length);
}

// Minimum-cost rectangular assignment. The implementation supports either
// orientation so callers may have more agents than currently available tasks.
std::vector<int> hungarian(const std::vector<std::vector<float>>& cost)
{
    int rows = static_cast<int>(cost.size());
    if (rows == 0) return {};
    int cols = static_cast<int>(cost.front().size());
    if (cols == 0) return std::vector<int>(rows, -1);

    const bool transposed = rows > cols;
    std::vector<std::vector<float>> matrix;
    if (transposed) {
        matrix.assign(cols, std::vector<float>(rows));
        for (int i = 0; i < rows; ++i)
            for (int j = 0; j < cols; ++j) matrix[j][i] = cost[i][j];
        std::swap(rows, cols);
    } else {
        matrix = cost;
    }

    std::vector<float> u(rows + 1), v(cols + 1);
    std::vector<int> p(cols + 1), way(cols + 1);
    for (int i = 1; i <= rows; ++i) {
        p[0] = i;
        int j0 = 0;
        std::vector<float> minv(cols + 1, std::numeric_limits<float>::infinity());
        std::vector<bool> used(cols + 1, false);
        do {
            used[j0] = true;
            const int i0 = p[j0];
            float delta = std::numeric_limits<float>::infinity();
            int j1 = 0;
            for (int j = 1; j <= cols; ++j) {
                if (used[j]) continue;
                const float current = matrix[i0 - 1][j - 1] - u[i0] - v[j];
                if (current < minv[j]) {
                    minv[j] = current;
                    way[j] = j0;
                }
                if (minv[j] < delta) {
                    delta = minv[j];
                    j1 = j;
                }
            }
            for (int j = 0; j <= cols; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);
        do {
            const int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }

    std::vector<int> assignment(rows, -1);
    for (int j = 1; j <= cols; ++j) {
        if (p[j] > 0) assignment[p[j] - 1] = j - 1;
    }
    if (!transposed) return assignment;

    std::vector<int> original(cols, -1);
    for (int task = 0; task < rows; ++task) {
        if (assignment[task] >= 0) original[assignment[task]] = task;
    }
    return original;
}

std::vector<std::pair<int, int>> top_k_match(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, float dist_weight, float task_length_weight,
    int candidate_top_k, std::chrono::steady_clock::time_point deadline)
{
    struct OrderedAgent { int index; int nearest_manhattan; };
    std::vector<OrderedAgent> order;
    order.reserve(agents.size());
    const int cols = std::max(1, env->cols);
    for (int i = 0; i < static_cast<int>(agents.size()); ++i) {
        if (std::chrono::steady_clock::now() >= deadline) break;
        int nearest = std::numeric_limits<int>::max();
        for (const TaskInfo& task : tasks) {
            nearest = std::min(nearest, std::abs(agents[i].location / cols - task.pickup / cols) +
                                            std::abs(agents[i].location % cols - task.pickup % cols));
        }
        order.push_back({i, nearest});
    }
    std::sort(order.begin(), order.end(), [](const OrderedAgent& lhs, const OrderedAgent& rhs) {
        return lhs.nearest_manhattan > rhs.nearest_manhattan;
    });

    std::vector<bool> used(tasks.size(), false);
    std::vector<std::pair<int, int>> result;
    for (const OrderedAgent& ordered : order) {
        if (std::chrono::steady_clock::now() >= deadline) break;
        const AgentInfo& agent = agents[ordered.index];
        std::vector<std::pair<int, int>> rough;
        rough.reserve(tasks.size());
        for (int j = 0; j < static_cast<int>(tasks.size()); ++j) {
            if (used[j]) continue;
            rough.emplace_back(std::abs(agent.location / cols - tasks[j].pickup / cols) +
                                   std::abs(agent.location % cols - tasks[j].pickup % cols), j);
        }
        const int keep = std::min(std::max(1, candidate_top_k), static_cast<int>(rough.size()));
        if (keep == 0) continue;
        std::partial_sort(rough.begin(), rough.begin() + keep, rough.end());
        float best_score = kInvalidCost;
        int best = -1;
        for (int k = 0; k < keep && std::chrono::steady_clock::now() < deadline; ++k) {
            const int task_index = rough[k].second;
            const float score = task_score(env, agent.location, tasks[task_index],
                                           dist_weight, task_length_weight);
            if (score < best_score) {
                best_score = score;
                best = task_index;
            }
        }
        if (best >= 0 && best_score < kInvalidCost / 2.0F) {
            used[best] = true;
            result.emplace_back(agent.id, tasks[best].id);
        }
    }
    return result;
}

TaskInfo make_task_info(int id, const Task& task)
{
    const int index = std::max(0, task.idx_next_loc);
    return {id, task.locations.at(index),
            std::vector<int>(task.locations.begin() + index, task.locations.end())};
}

}  // namespace

void schedule_plan_portable_task_matcher(int time_limit_ms,
                                         std::vector<int>& proposed_schedule,
                                         SharedEnvironment* env,
                                         const PortableTaskMatcherConfig& config)
{
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(std::max(0, time_limit_ms));
    std::vector<int> local = env->curr_task_schedule;
    local.resize(env->num_of_agents, -1);
    std::unordered_set<int> locked_tasks;
    std::unordered_map<int, int> old_assignment;
    std::vector<AgentInfo> candidates;

    for (int agent = 0; agent < env->num_of_agents; ++agent) {
        const int task_id = local[agent];
        if (task_id < 0) {
            candidates.push_back({agent, env->curr_states.at(agent).location});
            continue;
        }
        const auto task_it = env->task_pool.find(task_id);
        if (task_it == env->task_pool.end() || task_it->second.idx_next_loc < 0 ||
            task_it->second.idx_next_loc >= static_cast<int>(task_it->second.locations.size())) {
            local[agent] = -1;
            candidates.push_back({agent, env->curr_states.at(agent).location});
            continue;
        }
        const Task& task = task_it->second;
        const int pickup = task.locations.at(task.idx_next_loc);
        const int distance = get_h(env, env->curr_states.at(agent).location, pickup);
        if (!config.reassign_enabled || task.idx_next_loc > 0 ||
            (config.reassign_min_dist > 0 && distance < config.reassign_min_dist)) {
            locked_tasks.insert(task_id);
            continue;
        }
        candidates.push_back({agent, env->curr_states.at(agent).location});
        old_assignment[agent] = task_id;
        local[agent] = -1;
    }

    std::vector<TaskInfo> tasks;
    tasks.reserve(env->task_pool.size());
    for (const auto& entry : env->task_pool) {
        const Task& task = entry.second;
        if (locked_tasks.count(entry.first) || task.idx_next_loc < 0 ||
            task.idx_next_loc >= static_cast<int>(task.locations.size())) continue;
        tasks.push_back(make_task_info(entry.first, task));
    }
    if (candidates.empty() || tasks.empty() || std::chrono::steady_clock::now() >= deadline) {
        proposed_schedule = std::move(local);
        return;
    }

    std::vector<std::pair<int, int>> matches;
    const long long matrix_size = static_cast<long long>(candidates.size()) * tasks.size();
    if (matrix_size <= std::max(1, config.max_matrix_elements) &&
        std::chrono::steady_clock::now() < deadline) {
        std::vector<std::vector<float>> cost(candidates.size(),
                                             std::vector<float>(tasks.size(), kInvalidCost));
        for (int i = 0; i < static_cast<int>(candidates.size()) &&
                        std::chrono::steady_clock::now() < deadline; ++i) {
            for (int j = 0; j < static_cast<int>(tasks.size()); ++j) {
                float value = task_score(env, candidates[i].location, tasks[j], config.dist_weight,
                                         config.task_length_weight);
                const auto old = old_assignment.find(candidates[i].id);
                if (old != old_assignment.end() && old->second == tasks[j].id) {
                    value -= config.reassign_keep_bias;
                }
                cost[i][j] = value;
            }
        }
        if (std::chrono::steady_clock::now() < deadline) {
            const std::vector<int> assignment = hungarian(cost);
            for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
                const int j = i < static_cast<int>(assignment.size()) ? assignment[i] : -1;
                if (j >= 0 && j < static_cast<int>(tasks.size()) && cost[i][j] < kInvalidCost / 2.0F) {
                    matches.emplace_back(candidates[i].id, tasks[j].id);
                }
            }
        }
    }
    if (matches.empty() && std::chrono::steady_clock::now() < deadline) {
        matches = top_k_match(env, candidates, tasks, config.dist_weight, config.task_length_weight,
                              config.candidate_top_k, deadline);
    }
    if (matches.empty()) {
        // A scheduler timeout must not discard a still-valid unopened task.
        for (const auto& entry : old_assignment) local[entry.first] = entry.second;
    } else {
        for (const auto& match : matches) local[match.first] = match.second;
    }
    proposed_schedule = std::move(local);
}

void schedule_plan_portable_capped_hungarian(
    int time_limit_ms, std::vector<int>& proposed_schedule, SharedEnvironment* env,
    const PortableCappedHungarianConfig& config)
{
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(std::max(0, time_limit_ms));
    std::vector<int> local = env->curr_task_schedule;
    local.resize(env->num_of_agents, -1);
    std::unordered_set<int> assigned;
    std::vector<AgentInfo> agents;
    for (int agent = 0; agent < env->num_of_agents; ++agent) {
        if (local[agent] >= 0) assigned.insert(local[agent]);
        else agents.push_back({agent, env->curr_states.at(agent).location});
    }
    std::sort(agents.begin(), agents.end(), [](const AgentInfo& lhs, const AgentInfo& rhs) {
        return lhs.id < rhs.id;
    });
    agents.resize(std::min(static_cast<int>(agents.size()), std::max(1, config.max_agents)));

    std::vector<TaskInfo> tasks;
    for (const auto& entry : env->task_pool) {
        const Task& task = entry.second;
        if (assigned.count(entry.first) || task.idx_next_loc < 0 ||
            task.idx_next_loc >= static_cast<int>(task.locations.size())) continue;
        tasks.push_back(make_task_info(entry.first, task));
    }
    std::sort(tasks.begin(), tasks.end(), [&](const TaskInfo& lhs, const TaskInfo& rhs) {
        return task_score(env, lhs.pickup, lhs, 0.0F) < task_score(env, rhs.pickup, rhs, 0.0F);
    });
    tasks.resize(std::min(static_cast<int>(tasks.size()), std::max(1, config.max_tasks)));
    if (agents.empty() || tasks.empty() || std::chrono::steady_clock::now() >= deadline) {
        proposed_schedule = std::move(local);
        return;
    }

    std::vector<std::vector<float>> cost(agents.size(), std::vector<float>(tasks.size(), kInvalidCost));
    for (int i = 0; i < static_cast<int>(agents.size()) && std::chrono::steady_clock::now() < deadline; ++i) {
        for (int j = 0; j < static_cast<int>(tasks.size()); ++j) {
            cost[i][j] = task_score(env, agents[i].location, tasks[j], config.dist_weight,
                                    config.task_length_weight);
        }
    }
    if (std::chrono::steady_clock::now() < deadline) {
        const std::vector<int> assignment = hungarian(cost);
        for (int i = 0; i < static_cast<int>(agents.size()); ++i) {
            const int j = i < static_cast<int>(assignment.size()) ? assignment[i] : -1;
            if (j >= 0 && j < static_cast<int>(tasks.size()) && cost[i][j] < kInvalidCost / 2.0F) {
                local[agents[i].id] = tasks[j].id;
            }
        }
    }
    proposed_schedule = std::move(local);
}

void schedule_plan_portable_stable_snatch_hungarian(
    int time_limit_ms, std::vector<int>& proposed_schedule, SharedEnvironment* env,
    const PortableStableSnatchHungarianConfig& config)
{
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(std::max(0, time_limit_ms));
    std::vector<int> local = env->curr_task_schedule;
    local.resize(env->num_of_agents, -1);
    std::unordered_set<int> locked_tasks;
    std::unordered_map<int, int> incumbent_tasks;
    std::vector<AgentInfo> candidates;

    for (int agent = 0; agent < env->num_of_agents; ++agent) {
        const int task_id = local[agent];
        if (task_id < 0) {
            candidates.push_back({agent, env->curr_states.at(agent).location});
            continue;
        }
        const auto task_it = env->task_pool.find(task_id);
        if (task_it == env->task_pool.end() || task_it->second.idx_next_loc != 0 ||
            task_it->second.locations.empty()) {
            locked_tasks.insert(task_id);
            continue;
        }
        const int pickup_distance = get_h(env, env->curr_states.at(agent).location,
                                          task_it->second.locations.front());
        if (pickup_distance <= config.snatch_min_pickup_distance ||
            pickup_distance >= std::numeric_limits<int>::max() / 8) {
            locked_tasks.insert(task_id);
            continue;
        }
        candidates.push_back({agent, env->curr_states.at(agent).location});
        incumbent_tasks[agent] = task_id;
    }

    std::vector<TaskInfo> tasks;
    tasks.reserve(env->task_pool.size());
    for (const auto& entry : env->task_pool) {
        const Task& task = entry.second;
        if (locked_tasks.count(entry.first) || task.idx_next_loc < 0 ||
            task.idx_next_loc >= static_cast<int>(task.locations.size())) continue;
        tasks.push_back(make_task_info(entry.first, task));
    }
    std::sort(candidates.begin(), candidates.end(), [](const AgentInfo& lhs, const AgentInfo& rhs) {
        return lhs.id < rhs.id;
    });
    // Truncating this candidate set could duplicate an incumbent task: an
    // excluded incumbent would retain its assignment while its task remained
    // eligible for another candidate. Refuse the call instead.
    if (static_cast<int>(candidates.size()) > config.max_agents ||
        static_cast<int>(tasks.size()) > config.max_tasks) {
        proposed_schedule = std::move(local);
        return;
    }
    if (candidates.empty() || tasks.empty() || std::chrono::steady_clock::now() >= deadline) {
        proposed_schedule = std::move(local);
        return;
    }

    std::unordered_map<int, int> task_index;
    task_index.reserve(tasks.size());
    for (int j = 0; j < static_cast<int>(tasks.size()); ++j) task_index[tasks[j].id] = j;

    std::vector<std::vector<float>> cost(candidates.size(),
                                         std::vector<float>(tasks.size(), kInvalidCost));
    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        for (int j = 0; j < static_cast<int>(tasks.size()); ++j) {
            if (std::chrono::steady_clock::now() >= deadline) {
                proposed_schedule = std::move(local);
                return;
            }
            cost[i][j] = task_score(env, candidates[i].location, tasks[j], config.dist_weight,
                                    config.task_length_weight);
        }
    }

    // A different agent can take an unopened incumbent task only after a
    // material individual-cost reduction, matching the contest stable-snatch rule.
    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        const auto incumbent = incumbent_tasks.find(candidates[i].id);
        if (incumbent == incumbent_tasks.end()) continue;
        const auto task_it = task_index.find(incumbent->second);
        if (task_it == task_index.end()) continue;
        const int incumbent_task_index = task_it->second;
        const float incumbent_cost = cost[i][incumbent_task_index];
        if (incumbent_cost >= kInvalidCost / 2.0F) continue;
        const float required_improvement = std::max(
            config.snatch_min_abs_improve,
            std::abs(incumbent_cost) * config.snatch_min_rel_improve);
        const float max_snatch_cost = incumbent_cost - required_improvement;
        for (int other = 0; other < static_cast<int>(candidates.size()); ++other) {
            if (other != i && cost[other][incumbent_task_index] > max_snatch_cost)
                cost[other][incumbent_task_index] = kInvalidCost;
        }
    }
    if (std::chrono::steady_clock::now() >= deadline) {
        proposed_schedule = std::move(local);
        return;
    }

    const std::vector<int> assignment = hungarian(cost);
    for (const AgentInfo& candidate : candidates) local[candidate.id] = -1;
    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        const int j = i < static_cast<int>(assignment.size()) ? assignment[i] : -1;
        if (j >= 0 && j < static_cast<int>(tasks.size()) && cost[i][j] < kInvalidCost / 2.0F)
            local[candidates[i].id] = tasks[j].id;
    }
    proposed_schedule = std::move(local);
}

}  // namespace DefaultPlanner
