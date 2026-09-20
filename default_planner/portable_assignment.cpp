#include "portable_assignment.h"

#include "heuristics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <queue>
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
    int revealed_at;
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

float matcher_score(SharedEnvironment* env, int agent_location, const TaskInfo& task,
                    const PortableTaskMatcherConfig& config)
{
    const float base = task_score(env, agent_location, task, config.dist_weight);
    if (base >= kInvalidCost / 2.0F || config.wait_priority_weight <= 0.0F) return base;
    const int waited = std::max(0, env->curr_timestep - task.revealed_at -
                                    std::max(0, config.wait_priority_threshold));
    return base - config.wait_priority_weight * static_cast<float>(waited);
}

int task_length(SharedEnvironment* env, const TaskInfo& task)
{
    int length = 0;
    for (size_t i = 1; i < task.locations.size(); ++i) {
        const int segment = get_h(env, task.locations[i - 1], task.locations[i]);
        if (segment >= std::numeric_limits<int>::max() / 8) return -1;
        length += segment;
    }
    return length;
}

float recent_wait_heat(const SharedEnvironment* env, int location)
{
    if (location < 0 || location >= env->map.size() ||
        env->past_waitings.size() < static_cast<size_t>((location + 1) * 5)) return 0.0F;
    double total = 0.0;
    int observed = 0;
    for (int action = 1; action <= 4; ++action) {
        const auto& waiting = env->past_waitings[location * 5 + action];
        if (waiting.second <= 0.0) continue;
        total += waiting.first / waiting.second;
        ++observed;
    }
    return observed == 0 ? 0.0F : static_cast<float>(total / observed);
}

float flow_traffic_edge_cost(SharedEnvironment* env, const std::vector<Double4>& background_flow,
                             int location, int next, float congestion_weight,
                             bool use_wait_heat, float wait_heat_weight)
{
    if (background_flow.size() != env->map.size()) return 1;
    const int direction = get_d(location - next, env);
    const int contraflow = (background_flow[location].d[direction] + 1) *
                           background_flow[next].d[(direction + 2) % 4];
    int incoming = 0;
    for (int d = 0; d < 4; ++d) incoming += background_flow[next].d[d];
    float cost = 1.0F + std::max(0.0F, congestion_weight) *
        static_cast<float>(contraflow + incoming / 2);
    if (use_wait_heat && wait_heat_weight > 0.0F) {
        cost += wait_heat_weight * (recent_wait_heat(env, location) +
                                    0.5F * recent_wait_heat(env, next));
    }
    return cost;
}

std::vector<std::vector<float>> traffic_cost_matrix(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const PortableTaskMatcherConfig& config,
    const std::vector<Double4>& background_flow, std::chrono::steady_clock::time_point deadline)
{
    std::vector<std::vector<float>> cost(agents.size(),
                                         std::vector<float>(tasks.size(), kInvalidCost));
    const float task_pressure = agents.empty() ? 0.0F :
        static_cast<float>(tasks.size()) / static_cast<float>(agents.size());
    const bool use_wait_heat = config.use_wait_heat &&
        task_pressure >= std::max(0.0F, config.wait_heat_pressure_threshold);
    const int keep = std::min(std::max(1, config.traffic_top_k), static_cast<int>(tasks.size()));
    std::vector<int> lengths(tasks.size(), -2);

    for (int i = 0; i < static_cast<int>(agents.size()) &&
                    std::chrono::steady_clock::now() < deadline; ++i) {
        std::vector<std::pair<int, int>> nearest;
        nearest.reserve(tasks.size());
        for (int j = 0; j < static_cast<int>(tasks.size()); ++j) {
            const int distance = get_h(env, agents[i].location, tasks[j].pickup);
            if (distance < std::numeric_limits<int>::max() / 8) nearest.emplace_back(distance, j);
        }
        if (nearest.empty()) continue;
        const int selected = std::min(keep, static_cast<int>(nearest.size()));
        std::partial_sort(nearest.begin(), nearest.begin() + selected, nearest.end());

        std::unordered_map<int, std::vector<int>> task_indices_at_location;
        for (int k = 0; k < selected; ++k) {
            task_indices_at_location[tasks[nearest[k].second].pickup].push_back(nearest[k].second);
        }
        int remaining_goals = static_cast<int>(task_indices_at_location.size());
        std::vector<float> distance(env->map.size(), std::numeric_limits<float>::infinity());
        std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>,
                            std::greater<std::pair<float, int>>> open;
        distance[agents[i].location] = 0.0F;
        open.emplace(0.0F, agents[i].location);

        while (!open.empty() && remaining_goals > 0 &&
               std::chrono::steady_clock::now() < deadline) {
            const auto [current_cost, location] = open.top();
            open.pop();
            if (current_cost > distance[location]) continue;
            const auto goal = task_indices_at_location.find(location);
            if (goal != task_indices_at_location.end()) {
                for (const int task_index : goal->second) {
                    if (lengths[task_index] == -2) lengths[task_index] = task_length(env, tasks[task_index]);
                    if (lengths[task_index] >= 0) {
                        cost[i][task_index] = config.dist_weight * static_cast<float>(current_cost) +
                                              static_cast<float>(lengths[task_index]) -
                                              std::max(0.0F, config.wait_priority_weight) *
                                              static_cast<float>(std::max(0, env->curr_timestep -
                                                  tasks[task_index].revealed_at -
                                                  std::max(0, config.wait_priority_threshold)));
                    }
                }
                task_indices_at_location.erase(goal);
                --remaining_goals;
            }
            for (const int next : global_neighbors.at(location)) {
                const float next_cost = current_cost + flow_traffic_edge_cost(
                    env, background_flow, location, next, config.traffic_congestion_weight,
                    use_wait_heat, config.wait_heat_weight);
                if (next_cost < distance[next]) {
                    distance[next] = next_cost;
                    open.emplace(next_cost, next);
                }
            }
        }
    }
    return cost;
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
    return {id, task.locations.at(index), task.t_revealed,
            std::vector<int>(task.locations.begin() + index, task.locations.end())};
}

}  // namespace

void schedule_plan_portable_task_matcher(int time_limit_ms,
                                          std::vector<int>& proposed_schedule,
                                          SharedEnvironment* env,
                                          const PortableTaskMatcherConfig& config,
                                          const std::vector<Double4>& background_flow)
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
        std::vector<std::vector<float>> cost;
        if (config.use_traffic_cost) {
            cost = traffic_cost_matrix(env, candidates, tasks, config, background_flow, deadline);
        } else {
            cost.assign(candidates.size(), std::vector<float>(tasks.size(), kInvalidCost));
            for (int i = 0; i < static_cast<int>(candidates.size()) &&
                            std::chrono::steady_clock::now() < deadline; ++i) {
                for (int j = 0; j < static_cast<int>(tasks.size()); ++j) {
                    cost[i][j] = matcher_score(env, candidates[i].location, tasks[j], config);
                }
            }
        }
        for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
            for (int j = 0; j < static_cast<int>(tasks.size()); ++j) {
                const auto old = old_assignment.find(candidates[i].id);
                if (old != old_assignment.end() && old->second == tasks[j].id &&
                    cost[i][j] < kInvalidCost / 2.0F) {
                    cost[i][j] -= config.reassign_keep_bias;
                }
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
        matches = top_k_match(env, candidates, tasks, config.dist_weight, 1.0F,
                              config.candidate_top_k, deadline);
    }
    if (matches.empty()) {
        // A scheduler timeout must not discard a still-valid unopened task.
        for (const auto& entry : old_assignment) local[entry.first] = entry.second;
    } else {
        float assign_ratio = std::clamp(config.max_assign_ratio, 0.0F, 1.0F);
        if (config.adaptive_assign_ratio && !candidates.empty()) {
            // Keep the partial matcher under normal load. When unpicked work
            // exceeds candidate capacity, progressively remove the throttle
            // so old assignments can be corrected before a backlog persists.
            const float task_pressure = static_cast<float>(tasks.size()) /
                                        static_cast<float>(candidates.size());
            const float overload = std::clamp(task_pressure - 1.0F, 0.0F, 1.0F);
            assign_ratio += (1.0F - assign_ratio) * overload;
        }
        const int max_assignments = std::max(
            1, static_cast<int>(std::ceil(assign_ratio *
                                           static_cast<float>(candidates.size()))));
        const bool is_partial = static_cast<int>(matches.size()) > max_assignments;
        if (is_partial) {
            std::unordered_map<int, int> agent_locations;
            std::unordered_map<int, const TaskInfo*> tasks_by_id;
            for (const AgentInfo& candidate : candidates) agent_locations[candidate.id] = candidate.location;
            for (const TaskInfo& task : tasks) tasks_by_id[task.id] = &task;

            std::vector<std::pair<float, std::pair<int, int>>> scored_matches;
            scored_matches.reserve(matches.size());
            for (const auto& match : matches) {
                float value = matcher_score(env, agent_locations.at(match.first),
                                            *tasks_by_id.at(match.second), config);
                const auto old = old_assignment.find(match.first);
                if (old != old_assignment.end() && old->second == match.second) {
                    value -= config.reassign_keep_bias;
                }
                scored_matches.emplace_back(value, match);
            }
            std::sort(scored_matches.begin(), scored_matches.end());
            matches.clear();
            matches.reserve(max_assignments);
            for (int i = 0; i < max_assignments; ++i) matches.push_back(scored_matches[i].second);
        }
        if (is_partial) {
            std::unordered_set<int> retained_tasks;
            for (const auto& match : matches) retained_tasks.insert(match.second);
            for (const auto& entry : old_assignment) {
                if (!retained_tasks.count(entry.second)) local[entry.first] = entry.second;
            }
        }
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

}  // namespace DefaultPlanner
