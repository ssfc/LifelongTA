#include "portable_greedy_heap.h"

#include "heuristics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
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

void PortableGreedyHeapScheduler::prepare_fair_landmarks(SharedEnvironment* env)
{
    if (fair_landmark_map_size_ == env->map.size() && !fair_landmarks_.empty()) return;
    fair_landmarks_.clear();
    fair_landmark_map_size_ = env->map.size();
    if (env->rows <= 0 || env->cols <= 0 || env->map.empty()) return;

    for (int gr = 0; gr < 3; ++gr) {
        for (int gc = 0; gc < 4; ++gc) {
            const int target_row = gr * (env->rows - 1) / 2;
            const int target_col = gc * (env->cols - 1) / 3;
            int anchor = -1;
            int best = std::numeric_limits<int>::max();
            for (int loc = 0; loc < static_cast<int>(env->map.size()); ++loc) {
                if (env->map[loc] != 0) continue;
                const int distance = std::abs(loc / env->cols - target_row) +
                                     std::abs(loc % env->cols - target_col);
                if (distance < best) {
                    best = distance;
                    anchor = loc;
                }
            }
            if (anchor < 0) continue;

            std::vector<int> distance(env->map.size(), MAX_TIMESTEP);
            std::queue<int> open;
            distance[anchor] = 0;
            open.push(anchor);
            while (!open.empty()) {
                const int location = open.front();
                open.pop();
                for (const int next : global_neighbors[location]) {
                    if (distance[next] != MAX_TIMESTEP) continue;
                    distance[next] = distance[location] + 1;
                    open.push(next);
                }
            }
            fair_landmarks_.push_back(std::move(distance));
        }
    }
}

void PortableGreedyHeapScheduler::prepare_exact_pickup_cache(
    SharedEnvironment* env, const std::vector<int>& pickup_sites)
{
    exact_pickup_distances_.clear();
    exact_pickup_index_.clear();
    if (pickup_sites.empty() || env->map.empty() ||
        pickup_sites.size() * env->map.size() * sizeof(int) > 256ULL * 1024 * 1024) {
        std::cerr << "heap_exact_pickup_cache disabled: no sites or memory limit exceeded\n";
        return;
    }

    std::vector<int> open;
    open.reserve(env->map.size());
    for (const int site : pickup_sites) {
        if (site < 0 || site >= static_cast<int>(env->map.size()) || env->map[site] != 0 ||
            exact_pickup_index_.count(site) != 0) continue;
        auto& distance = exact_pickup_distances_.emplace_back(env->map.size(), MAX_TIMESTEP);
        exact_pickup_index_[site] = static_cast<int>(exact_pickup_distances_.size()) - 1;
        open.clear();
        open.push_back(site);
        distance[site] = 0;
        for (size_t head = 0; head < open.size(); ++head) {
            const int location = open[head];
            for (const int next : global_neighbors[location]) {
                if (distance[next] != MAX_TIMESTEP) continue;
                distance[next] = distance[location] + 1;
                open.push_back(next);
            }
        }
    }
    std::cerr << "heap_exact_pickup_cache sites=" << exact_pickup_distances_.size()
              << " bytes=" << exact_pickup_distances_.size() * env->map.size() * sizeof(int) << '\n';
}

int PortableGreedyHeapScheduler::exact_distance(SharedEnvironment* env, int first, int second) const
{
    if (exact_pickup_cache_enabled_) {
        const auto first_it = exact_pickup_index_.find(first);
        if (first_it != exact_pickup_index_.end()) return exact_pickup_distances_[first_it->second][second];
        const auto second_it = exact_pickup_index_.find(second);
        if (second_it != exact_pickup_index_.end()) return exact_pickup_distances_[second_it->second][first];
    }
    return get_h(env, first, second);
}

int PortableGreedyHeapScheduler::fair_distance(SharedEnvironment* env, int first, int second) const
{
    int bound = std::abs(first / env->cols - second / env->cols) +
                std::abs(first % env->cols - second % env->cols);
    for (const auto& distance : fair_landmarks_) {
        if (distance[first] == MAX_TIMESTEP || distance[second] == MAX_TIMESTEP) continue;
        bound = std::max(bound, std::abs(distance[first] - distance[second]));
    }
    return bound;
}

float PortableGreedyHeapScheduler::score(SharedEnvironment* env, int agent_location,
                                         const TaskInfo& task, float dist_weight) const
{
    if (task.locations.empty()) return std::numeric_limits<float>::infinity();
    int distance = exact_distance(env, agent_location, task.start_location);
    if (task.cached_remaining_length < 0) {
        int length = 0;
        for (size_t i = 1; i < task.locations.size(); ++i) {
            length += exact_distance(env, task.locations[i - 1], task.locations[i]);
        }
        task.cached_remaining_length = length;
    }
    float value = dist_weight * static_cast<float>(distance) +
                  static_cast<float>(task.cached_remaining_length);
    if (flow_penalty_weight_ > 0.0f && flow_zone_rows_ > 0 && flow_zone_cols_ > 0) {
        if (task.cached_flow_penalty < 0.0f) {
            float total_penalty = 0.0f;
            int segments = 0;
            for (size_t i = 1; i < task.locations.size(); ++i) {
                total_penalty += zone_path_penalty(zone_of_location(env, task.locations[i - 1]),
                                                   zone_of_location(env, task.locations[i]));
                ++segments;
            }
            task.cached_flow_penalty = segments > 0 ? total_penalty / static_cast<float>(segments) : 0.0f;
        }
        value += flow_penalty_weight_ * task.cached_flow_penalty;
    }
    if (age_bonus_ > 0.0f && task.revealed_timestep >= 0) {
        const int waiting_time = std::max(0, current_timestep_ - task.revealed_timestep);
        value -= age_bonus_ * static_cast<float>(waiting_time);
    }
    return value;
}

int PortableGreedyHeapScheduler::zone_of_location(SharedEnvironment* env, int location) const
{
    if (flow_zone_rows_ <= 0 || flow_zone_cols_ <= 0 || location < 0 ||
        env->rows <= 0 || env->cols <= 0) {
        return -1;
    }
    const int row = location / env->cols;
    const int col = location % env->cols;
    if (row < 0 || row >= env->rows || col < 0 || col >= env->cols) return -1;
    const int zone_row = std::min(flow_zone_rows_ - 1, row * flow_zone_rows_ / env->rows);
    const int zone_col = std::min(flow_zone_cols_ - 1, col * flow_zone_cols_ / env->cols);
    return zone_row * flow_zone_cols_ + zone_col;
}

void PortableGreedyHeapScheduler::add_zone_path(int from_zone, int to_zone, float weight)
{
    if (from_zone < 0 || to_zone < 0 || from_zone >= static_cast<int>(opened_zone_flow_.size()) ||
        to_zone >= static_cast<int>(opened_zone_flow_.size()) || weight <= 0.0f) {
        return;
    }
    int row = from_zone / flow_zone_cols_;
    int col = from_zone % flow_zone_cols_;
    const int target_row = to_zone / flow_zone_cols_;
    const int target_col = to_zone % flow_zone_cols_;
    while (row != target_row) {
        const int direction = target_row > row ? 1 : 3;  // south or north
        opened_zone_flow_[row * flow_zone_cols_ + col][direction] += weight;
        row += target_row > row ? 1 : -1;
    }
    while (col != target_col) {
        const int direction = target_col > col ? 0 : 2;  // east or west
        opened_zone_flow_[row * flow_zone_cols_ + col][direction] += weight;
        col += target_col > col ? 1 : -1;
    }
}

float PortableGreedyHeapScheduler::zone_path_penalty(int from_zone, int to_zone) const
{
    if (from_zone < 0 || to_zone < 0 || from_zone >= static_cast<int>(opened_zone_flow_.size()) ||
        to_zone >= static_cast<int>(opened_zone_flow_.size())) {
        return 0.0f;
    }
    int row = from_zone / flow_zone_cols_;
    int col = from_zone % flow_zone_cols_;
    const int target_row = to_zone / flow_zone_cols_;
    const int target_col = to_zone % flow_zone_cols_;
    float penalty = 0.0f;
    int steps = 0;
    auto add_step = [&](int direction, int next_row, int next_col) {
        const int zone = row * flow_zone_cols_ + col;
        const int next_zone = next_row * flow_zone_cols_ + next_col;
        const int opposite = (direction + 2) % 4;
        const float local_load = opened_zone_flow_[zone][direction];
        const float opposing_load = opened_zone_flow_[next_zone][opposite];
        const float arrival_load = opened_zone_flow_[next_zone][0] + opened_zone_flow_[next_zone][1] +
                                   opened_zone_flow_[next_zone][2] + opened_zone_flow_[next_zone][3];
        penalty += local_load + opposing_load + 0.25f * arrival_load;
        ++steps;
        row = next_row;
        col = next_col;
    };
    while (row != target_row) {
        const int direction = target_row > row ? 1 : 3;
        add_step(direction, row + (target_row > row ? 1 : -1), col);
    }
    while (col != target_col) {
        const int direction = target_col > col ? 0 : 2;
        add_step(direction, row, col + (target_col > col ? 1 : -1));
    }
    return steps > 0 ? penalty / static_cast<float>(steps) : 0.0f;
}

void PortableGreedyHeapScheduler::build_opened_zone_flow(
    SharedEnvironment* env, const PortableGreedyHeapConfig& config)
{
    flow_zone_rows_ = std::max(0, config.flow_zone_rows);
    flow_zone_cols_ = std::max(0, config.flow_zone_cols);
    flow_penalty_weight_ = std::max(0.0f, config.flow_penalty_weight);
    opened_zone_flow_.clear();
    if (flow_penalty_weight_ <= 0.0f || flow_zone_rows_ <= 0 || flow_zone_cols_ <= 0 ||
        env->rows <= 0 || env->cols <= 0) {
        flow_zone_rows_ = 0;
        flow_zone_cols_ = 0;
        flow_penalty_weight_ = 0.0f;
        return;
    }
    opened_zone_flow_.assign(static_cast<size_t>(flow_zone_rows_ * flow_zone_cols_), {});
    for (const auto& entry : env->task_pool) {
        const Task& task = entry.second;
        if (task.idx_next_loc <= 0 || task.agent_assigned < 0 ||
            task.agent_assigned >= static_cast<int>(env->curr_states.size()) ||
            task.idx_next_loc >= static_cast<int>(task.locations.size())) {
            continue;
        }
        int previous = env->curr_states.at(task.agent_assigned).location;
        for (size_t i = static_cast<size_t>(task.idx_next_loc); i < task.locations.size(); ++i) {
            const int next = task.locations[i];
            add_zone_path(zone_of_location(env, previous), zone_of_location(env, next), 1.0f);
            previous = next;
        }
    }
}

void PortableGreedyHeapScheduler::rebuild_candidates(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, float dist_weight, int sort_k,
    std::chrono::steady_clock::time_point deadline)
{
    last_build_stats_ = {};
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
        last_build_stats_.evaluated_pairs += static_cast<long long>(list.size());
        if (list.size() == tasks.size()) ++last_build_stats_.full_scans;
        else if (!list.empty()) ++last_build_stats_.partial_scans;
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

void PortableGreedyHeapScheduler::rebuild_candidates_fair(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const PortableGreedyHeapConfig& config,
    std::chrono::steady_clock::time_point deadline)
{
    last_build_stats_ = {};
    candidates_.assign(agents.size(), {});
    candidate_agent_ids_.clear();
    candidate_agent_locations_.clear();
    candidate_task_ids_.clear();
    if (agents.empty() || tasks.empty()) return;

    const int zone_rows = std::min(env->rows, 40);
    const int zone_cols = std::min(env->cols, 40);
    if (zone_rows <= 0 || zone_cols <= 0) return;
    const int keep = std::min(std::max(1, config.fair_candidate_k),
                              static_cast<int>(tasks.size()));
    auto zone_row = [&](int location) { return (location / env->cols) * zone_rows / env->rows; };
    auto zone_col = [&](int location) { return (location % env->cols) * zone_cols / env->cols; };
    std::vector<std::vector<int>> zones(static_cast<size_t>(zone_rows * zone_cols));
    std::vector<float> task_base(tasks.size(), 0.0f);
    for (int ti = 0; ti < static_cast<int>(tasks.size()); ++ti) {
        const TaskInfo& task = tasks[ti];
        zones[zone_row(task.start_location) * zone_cols + zone_col(task.start_location)].push_back(ti);
        for (size_t i = 1; i < task.locations.size(); ++i) {
            task_base[ti] += static_cast<float>(exact_pickup_cache_enabled_
                ? exact_distance(env, task.locations[i - 1], task.locations[i])
                : fair_distance(env, task.locations[i - 1], task.locations[i]));
        }
        if (flow_penalty_weight_ > 0.0f && task.locations.size() > 1) {
            float pressure = 0.0f;
            for (size_t i = 1; i < task.locations.size(); ++i) {
                pressure += zone_path_penalty(zone_of_location(env, task.locations[i - 1]),
                                              zone_of_location(env, task.locations[i]));
            }
            task_base[ti] += flow_penalty_weight_ *
                pressure / static_cast<float>(task.locations.size() - 1);
        }
    }

    const size_t start = static_cast<size_t>(env->curr_timestep) * 7919U % agents.size();
    for (size_t offset = 0; offset < agents.size(); ++offset) {
        if ((offset & 63U) == 0U && std::chrono::steady_clock::now() >= deadline) break;
        const size_t ai = (start + offset) % agents.size();
        const int location = agents[ai].location;
        const int zr = zone_row(location);
        const int zc = zone_col(location);
        auto& list = candidates_[ai];
        list.reserve(keep);
        for (int radius = 0; radius <= 3 &&
                             (radius <= 1 || static_cast<int>(list.size()) < keep); ++radius) {
            for (int row = std::max(0, zr - radius); row <= std::min(zone_rows - 1, zr + radius); ++row) {
                for (int col = std::max(0, zc - radius); col <= std::min(zone_cols - 1, zc + radius); ++col) {
                    if (std::max(std::abs(row - zr), std::abs(col - zc)) != radius) continue;
                    for (const int ti : zones[row * zone_cols + col]) {
                        const float value = config.dist_weight * static_cast<float>(
                            exact_pickup_cache_enabled_
                                ? exact_distance(env, location, tasks[ti].start_location)
                                : fair_distance(env, location, tasks[ti].start_location)) + task_base[ti];
                        if (static_cast<int>(list.size()) < keep) {
                            list.push_back({value, ti});
                        } else {
                            auto worst = std::max_element(list.begin(), list.end(),
                                [](const Candidate& lhs, const Candidate& rhs) {
                                    return lhs.score < rhs.score;
                                });
                            if (value < worst->score) *worst = {value, ti};
                        }
                    }
                }
            }
        }
        if (list.empty()) {
            const int ti = static_cast<int>((ai + static_cast<size_t>(env->curr_timestep)) % tasks.size());
            list.push_back({config.dist_weight * static_cast<float>(exact_pickup_cache_enabled_
                ? exact_distance(env, location, tasks[ti].start_location)
                : fair_distance(env, location, tasks[ti].start_location)) + task_base[ti], ti});
        }
        std::sort(list.begin(), list.end(), [](const Candidate& lhs, const Candidate& rhs) {
            return lhs.score < rhs.score;
        });
        ++last_build_stats_.seeded_agents;
        last_build_stats_.evaluated_pairs += static_cast<long long>(list.size());
    }

    if (exact_pickup_cache_enabled_) {
        last_build_stats_.exact_agents = last_build_stats_.seeded_agents;
        return;
    }

    // Refine complete shortlists only; unrefined agents retain comparable
    // inexpensive scores and remain eligible for the heap match.
    for (size_t offset = 0; offset < agents.size() &&
                            std::chrono::steady_clock::now() < deadline; ++offset) {
        const size_t ai = (start + offset) % agents.size();
        auto& list = candidates_[ai];
        if (list.empty()) continue;
        std::vector<Candidate> refined = list;
        bool complete = true;
        for (Candidate& candidate : refined) {
            if (std::chrono::steady_clock::now() >= deadline) {
                complete = false;
                break;
            }
            candidate.score = score(env, agents[ai].location, tasks[candidate.task_index],
                                    config.dist_weight);
        }
        if (!complete) break;
        std::sort(refined.begin(), refined.end(), [](const Candidate& lhs, const Candidate& rhs) {
            return lhs.score < rhs.score;
        });
        list = std::move(refined);
        ++last_build_stats_.exact_agents;
    }
}

void PortableGreedyHeapScheduler::rerank_traffic_candidates(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const std::vector<Double4>& background_flow,
    const PortableGreedyHeapConfig& config, std::chrono::steady_clock::time_point deadline)
{
    const int top_k = std::max(0, config.traffic_rerank_top_k);
    const int max_steps = std::max(0, config.traffic_rerank_steps);
    const float weight = std::max(0.0f, config.traffic_rerank_weight);
    if (top_k <= 0 || max_steps <= 0 || weight <= 0.0f ||
        background_flow.size() != env->map.size()) {
        return;
    }

    auto edge_pressure = [&](int location, int next) {
        const int direction = get_d(location - next, env);
        const int opposite = (direction + 2) % 4;
        int incoming = 0;
        for (int d = 0; d < 4; ++d) incoming += background_flow[next].d[d];
        const float contraflow = static_cast<float>((background_flow[location].d[direction] + 1) *
                                                     background_flow[next].d[opposite]);
        return contraflow + 0.5f * static_cast<float>(incoming);
    };

    for (int ai = 0; ai < static_cast<int>(agents.size()) &&
                     std::chrono::steady_clock::now() < deadline; ++ai) {
        auto& list = candidates_[ai];
        const int limit = std::min(top_k, static_cast<int>(list.size()));
        for (int ci = 0; ci < limit && std::chrono::steady_clock::now() < deadline; ++ci) {
            const int task_index = list[ci].task_index;
            if (task_index < 0 || task_index >= static_cast<int>(tasks.size())) continue;
            const int target = tasks[task_index].start_location;
            int location = agents[ai].location;
            int distance = get_h(env, location, target);
            float pressure = 0.0f;
            int traversed = 0;
            while (traversed < max_steps && distance > 0 &&
                   std::chrono::steady_clock::now() < deadline) {
                int best_next = -1;
                int best_distance = distance;
                float best_pressure = std::numeric_limits<float>::infinity();
                for (const int next : global_neighbors.at(location)) {
                    const int next_distance = get_h(env, next, target);
                    if (next_distance >= best_distance) continue;
                    const float candidate_pressure = edge_pressure(location, next);
                    if (candidate_pressure < best_pressure ||
                        (candidate_pressure == best_pressure && next_distance < best_distance)) {
                        best_next = next;
                        best_distance = next_distance;
                        best_pressure = candidate_pressure;
                    }
                }
                if (best_next < 0) break;
                pressure += best_pressure;
                location = best_next;
                distance = best_distance;
                ++traversed;
            }
            if (traversed > 0) list[ci].score += weight * pressure / static_cast<float>(traversed);
        }
        std::sort(list.begin(), list.end(), [](const Candidate& lhs, const Candidate& rhs) {
            return lhs.score < rhs.score;
        });
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

void PortableGreedyHeapScheduler::refine_local_exchanges(
    SharedEnvironment* env, std::vector<Match>& matches, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const PortableGreedyHeapConfig& config,
    std::chrono::steady_clock::time_point deadline) const
{
    const int top_k = std::max(0, config.local_exchange_top_k);
    if (top_k <= 0 || matches.size() < 2 || candidates_.size() != agents.size()) return;

    std::unordered_map<int, int> agent_index;
    std::unordered_map<int, int> task_index;
    for (int i = 0; i < static_cast<int>(agents.size()); ++i) agent_index[agents[i].id] = i;
    for (int i = 0; i < static_cast<int>(tasks.size()); ++i) task_index[tasks[i].id] = i;

    // A deterministic scan covers the high-conflict pairs that random LNS is
    // unlikely to sample when thousands of agents are simultaneously matched.
    for (int pass = 0; pass < 2 && std::chrono::steady_clock::now() < deadline; ++pass) {
        std::unordered_map<int, int> match_by_agent;
        std::unordered_map<int, int> match_by_task;
        for (int mi = 0; mi < static_cast<int>(matches.size()); ++mi) {
            match_by_agent[matches[mi].first] = mi;
            match_by_task[matches[mi].second] = mi;
        }
        bool improved = false;
        for (int ai = 0; ai < static_cast<int>(agents.size()) &&
                         std::chrono::steady_clock::now() < deadline; ++ai) {
            const auto own_match = match_by_agent.find(agents[ai].id);
            if (own_match == match_by_agent.end()) continue;
            const int own_index = own_match->second;
            const int own_task_id = matches[own_index].second;
            const auto own_task = task_index.find(own_task_id);
            if (own_task == task_index.end()) continue;

            const int limit = std::min(top_k, static_cast<int>(candidates_[ai].size()));
            for (int ci = 0; ci < limit && std::chrono::steady_clock::now() < deadline; ++ci) {
                const int wanted_task_index = candidates_[ai][ci].task_index;
                if (wanted_task_index < 0 || wanted_task_index >= static_cast<int>(tasks.size())) continue;
                const int wanted_task_id = tasks[wanted_task_index].id;
                const auto owner_match = match_by_task.find(wanted_task_id);
                if (owner_match == match_by_task.end() || owner_match->second == own_index) continue;
                const int other_index = owner_match->second;
                const auto other_agent = agent_index.find(matches[other_index].first);
                if (other_agent == agent_index.end()) continue;

                const float before = score(env, agents[ai].location, tasks[own_task->second], config.dist_weight) +
                                     score(env, agents[other_agent->second].location, tasks[wanted_task_index],
                                           config.dist_weight);
                const float after = score(env, agents[ai].location, tasks[wanted_task_index], config.dist_weight) +
                                    score(env, agents[other_agent->second].location, tasks[own_task->second],
                                          config.dist_weight);
                if (after + 1e-4f < before) {
                    std::swap(matches[own_index].second, matches[other_index].second);
                    match_by_task[own_task_id] = other_index;
                    match_by_task[wanted_task_id] = own_index;
                    improved = true;
                    break;
                }
            }
        }
        if (!improved) break;
    }
}

void PortableGreedyHeapScheduler::schedule(int time_limit_ms, std::vector<int>& proposed_schedule,
                                           SharedEnvironment* env,
                                           const PortableGreedyHeapConfig& config,
                                           const std::vector<Double4>& background_flow)
{
    exact_pickup_cache_enabled_ = config.exact_pickup_cache && !exact_pickup_distances_.empty();
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
                                                   task.locations.end()),
                                  task.t_revealed});
        old_assignment[agent] = task_id;
    }

    for (const auto& entry : env->task_pool) {
        const int task_id = entry.first;
        const Task& task = entry.second;
        if (task.idx_next_loc < 0 || task.idx_next_loc >= static_cast<int>(task.locations.size())) continue;
        if (locked_tasks.count(task_id) || owner_by_task.count(task_id)) continue;
        free_tasks.push_back({task_id, task.locations.at(task.idx_next_loc),
                              std::vector<int>(task.locations.begin() + task.idx_next_loc,
                                               task.locations.end()),
                              task.t_revealed});
    }

    // Preserve active and stable assignments, then cap only newly assigned
    // agents. This mirrors the contest GreedyHeap throttling semantics.
    const float configured_ratio = env->curr_timestep < config.warmup_steps
        ? config.warmup_assign_ratio
        : config.max_assign_ratio;
    const float max_assign_ratio = std::clamp(configured_ratio, 0.0f, 1.0f);
    if (max_assign_ratio < 0.999f) {
        const int max_total = std::max(1, static_cast<int>(max_assign_ratio * agent_count));
        const int already_assigned = static_cast<int>(std::count_if(
            local.begin(), local.end(), [](int task_id) { return task_id >= 0; }));
        const int max_new = std::max(0, max_total - already_assigned);
        if (static_cast<int>(free_agents.size()) > max_new) {
            free_agents.resize(max_new);
        }
    }

    const int rebuild_ms = std::max(0, time_limit_ms) * std::clamp(config.rebuild_pct, 0, 100) / 100;
    const int lns_ms = std::max(0, time_limit_ms) * std::clamp(config.lns_pct, 0, 100) / 100;
    const auto rebuild_deadline = std::min(deadline, start + std::chrono::milliseconds(rebuild_ms));
    const auto lns_deadline = deadline - std::chrono::milliseconds(std::min(lns_ms, std::max(0, time_limit_ms)));

    build_opened_zone_flow(env, config);
    age_bonus_ = std::max(0.0f, config.age_bonus);
    current_timestep_ = env->curr_timestep;
    const bool traffic_rerank_enabled = config.traffic_rerank_top_k > 0 &&
        config.traffic_rerank_steps > 0 && config.traffic_rerank_weight > 0.0f;
    if (flow_penalty_weight_ > 0.0f || age_bonus_ > 0.0f || traffic_rerank_enabled) {
        // Candidate scores depend on flow snapshots and task ages.
        candidates_.clear();
        candidate_agent_ids_.clear();
        candidate_agent_locations_.clear();
        candidate_task_ids_.clear();
    }
    if (config.fair_candidate_k > 0 &&
        static_cast<int>(free_agents.size()) > std::max(0, config.fair_exact_agent_threshold)) {
        rebuild_candidates_fair(env, free_agents, free_tasks, config, rebuild_deadline);
    } else {
        rebuild_candidates(env, free_agents, free_tasks, config.dist_weight, config.sort_k,
                           rebuild_deadline);
    }
    if (config.candidate_diag_every > 0 &&
        env->curr_timestep % config.candidate_diag_every == 0) {
        std::array<int, 4> covered{};
        for (size_t ai = 0; ai < candidates_.size(); ++ai) {
            if (!candidates_[ai].empty()) ++covered[ai * covered.size() / candidates_.size()];
        }
        std::cerr << "heap_candidate_coverage timestep=" << env->curr_timestep
                  << " agents=" << free_agents.size() << " tasks=" << free_tasks.size()
                  << " quartiles=" << covered[0] << ',' << covered[1] << ','
                  << covered[2] << ',' << covered[3]
                  << " full_scans=" << last_build_stats_.full_scans
                  << " partial_scans=" << last_build_stats_.partial_scans
                  << " seeded_agents=" << last_build_stats_.seeded_agents
                  << " exact_agents=" << last_build_stats_.exact_agents
                  << " evaluated_pairs=" << last_build_stats_.evaluated_pairs << '\n';
    }
    rerank_traffic_candidates(env, free_agents, free_tasks, background_flow, config, lns_deadline);
    std::vector<Match> matches = lazy_match(free_agents, free_tasks, lns_deadline);
    refine_local_exchanges(env, matches, free_agents, free_tasks, config, lns_deadline);

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

namespace {
PortableGreedyHeapScheduler& portable_scheduler()
{
    static PortableGreedyHeapScheduler scheduler;
    return scheduler;
}
}  // namespace

void schedule_plan_portable_greedy_heap(int time_limit_ms, std::vector<int>& proposed_schedule,
                                        SharedEnvironment* env,
                                        const PortableGreedyHeapConfig& config,
                                        const std::vector<Double4>& background_flow)
{
    portable_scheduler().schedule(time_limit_ms, proposed_schedule, env, config, background_flow);
}

void prepare_portable_greedy_heap(SharedEnvironment* env,
                                  const PortableGreedyHeapConfig& config,
                                  const std::vector<int>& pickup_sites)
{
    if (config.exact_pickup_cache) {
        portable_scheduler().prepare_exact_pickup_cache(env, pickup_sites);
    } else if (config.fair_candidate_k > 0) {
        portable_scheduler().prepare_fair_landmarks(env);
    }
}

}  // namespace DefaultPlanner
