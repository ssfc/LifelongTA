#include "portable_greedy_heap.h"

#include "heuristics.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
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

namespace {

float flow_density(const std::vector<Double4>& background_flow, int location)
{
    if (location < 0 || location >= static_cast<int>(background_flow.size())) return 0.0f;
    float density = 0.0f;
    for (const double value : background_flow[location].d) {
        density += static_cast<float>(value);
    }
    return density;
}

class ZoneFutureFlow {
public:
    ZoneFutureFlow(SharedEnvironment* env, const std::vector<Double4>& background_flow,
                   int cell_size)
        : cols_(std::max(1, env->cols)), rows_(std::max(1, env->rows)),
          cell_size_(std::max(1, cell_size)),
          zone_cols_((cols_ + cell_size_ - 1) / cell_size_),
          zone_rows_((rows_ + cell_size_ - 1) / cell_size_),
          flow_(zone_rows_ * zone_cols_) {
        if (background_flow.size() != env->map.size()) return;
        for (int loc = 0; loc < static_cast<int>(background_flow.size()); ++loc) {
            const int z = zone_of(loc);
            for (int dir = 0; dir < 4; ++dir) {
                flow_[z][dir] += static_cast<float>(background_flow[loc].d[dir]);
            }
        }
    }

    float route_penalty(int source, const std::vector<int>& locations,
                        float hard_cap, bool* blocked) const {
        if (blocked) *blocked = false;
        float penalty = 0.0f;
        int previous = source;
        for (const int target : locations) {
            penalty += segment_penalty(previous, target, hard_cap, blocked);
            if (blocked && *blocked) return penalty;
            previous = target;
        }
        return penalty;
    }

    void add_route(int source, const std::vector<int>& locations) {
        int previous = source;
        int step = 0;
        for (const int target : locations) {
            add_segment(previous, target, step);
            previous = target;
        }
    }

private:
    int zone_of(int location) const {
        const int row = std::clamp(location / cols_, 0, rows_ - 1);
        const int col = std::clamp(location % cols_, 0, cols_ - 1);
        return (row / cell_size_) * zone_cols_ + col / cell_size_;
    }

    static int total_flow(const std::array<float, 4>& flow) {
        float total = 0.0f;
        for (const float value : flow) total += value;
        return static_cast<int>(std::lround(total));
    }

    template <typename Visitor>
    void walk_zones(int source, int target, Visitor&& visit) const {
        int row = zone_of(source) / zone_cols_;
        int col = zone_of(source) % zone_cols_;
        const int target_zone = zone_of(target);
        const int target_row = target_zone / zone_cols_;
        const int target_col = target_zone % zone_cols_;
        while (col != target_col) {
            const int direction = col < target_col ? 0 : 2;
            const int next_col = col + (direction == 0 ? 1 : -1);
            visit(row * zone_cols_ + col, row * zone_cols_ + next_col, direction);
            col = next_col;
        }
        while (row != target_row) {
            const int direction = row < target_row ? 1 : 3;
            const int next_row = row + (direction == 1 ? 1 : -1);
            visit(row * zone_cols_ + col, next_row * zone_cols_ + col, direction);
            row = next_row;
        }
    }

    float segment_penalty(int source, int target, float hard_cap, bool* blocked) const {
        float penalty = 0.0f;
        walk_zones(source, target, [&](int zone, int next_zone, int direction) {
            const int opposite = (direction + 2) % 4;
            const float same = flow_[zone][direction];
            const float contraflow = flow_[next_zone][opposite];
            const float density = static_cast<float>(total_flow(flow_[next_zone]));
            if (hard_cap > 0.0f && density > hard_cap && contraflow > same) {
                if (blocked) *blocked = true;
                return;
            }
            penalty += contraflow + 0.15f * density;
        });
        return penalty;
    }

    void add_segment(int source, int target, int& step) {
        walk_zones(source, target, [&](int zone, int, int direction) {
            flow_[zone][direction] += 1.0f / (1.0f + 0.1f * static_cast<float>(step++));
        });
    }

    int cols_;
    int rows_;
    int cell_size_;
    int zone_cols_;
    int zone_rows_;
    std::vector<std::array<float, 4>> flow_;
};

class OpenedTaskFlow {
public:
    OpenedTaskFlow(SharedEnvironment* env, int zone_rows, int zone_cols)
        : env_(env), zone_rows_(std::max(1, zone_rows)), zone_cols_(std::max(1, zone_cols)),
          flow_(static_cast<size_t>(zone_rows_ * zone_cols_)) {}

    void add_path(int source, const std::vector<int>& locations) {
        int previous = source;
        for (const int target : locations) {
            walk(previous, target, [&](int zone, int, int direction) {
                flow_[zone][direction] += 1.0f;
            });
            previous = target;
        }
    }

    float path_penalty(const std::vector<int>& locations) const {
        float penalty = 0.0f;
        int steps = 0;
        for (size_t i = 1; i < locations.size(); ++i) {
            walk(locations[i - 1], locations[i], [&](int zone, int next_zone, int direction) {
                const int opposite = (direction + 2) % 4;
                const float arrival_load = std::accumulate(
                    flow_[next_zone].begin(), flow_[next_zone].end(), 0.0f);
                penalty += flow_[zone][direction] + flow_[next_zone][opposite] + 0.25f * arrival_load;
                ++steps;
            });
        }
        return steps > 0 ? penalty / static_cast<float>(steps) : 0.0f;
    }

private:
    int zone_of(int location) const {
        const int row = std::clamp(location / std::max(1, env_->cols), 0, std::max(0, env_->rows - 1));
        const int col = std::clamp(location % std::max(1, env_->cols), 0, std::max(0, env_->cols - 1));
        const int zone_row = std::min(zone_rows_ - 1, row * zone_rows_ / std::max(1, env_->rows));
        const int zone_col = std::min(zone_cols_ - 1, col * zone_cols_ / std::max(1, env_->cols));
        return zone_row * zone_cols_ + zone_col;
    }

    template <typename Visitor>
    void walk(int source, int target, Visitor&& visit) const {
        int row = zone_of(source) / zone_cols_;
        int col = zone_of(source) % zone_cols_;
        const int target_zone = zone_of(target);
        const int target_row = target_zone / zone_cols_;
        const int target_col = target_zone % zone_cols_;
        while (row != target_row) {
            const int direction = target_row > row ? 1 : 3;
            const int next_row = row + (direction == 1 ? 1 : -1);
            visit(row * zone_cols_ + col, next_row * zone_cols_ + col, direction);
            row = next_row;
        }
        while (col != target_col) {
            const int direction = target_col > col ? 0 : 2;
            const int next_col = col + (direction == 0 ? 1 : -1);
            visit(row * zone_cols_ + col, row * zone_cols_ + next_col, direction);
            col = next_col;
        }
    }

    SharedEnvironment* env_;
    int zone_rows_;
    int zone_cols_;
    std::vector<std::array<float, 4>> flow_;
};

}  // namespace

float PortableGreedyHeapScheduler::score(SharedEnvironment* env, int agent_location,
                                         const TaskInfo& task,
                                         const PortableGreedyHeapConfig& config,
                                         const std::vector<Double4>& background_flow) const
{
    if (task.locations.empty()) return std::numeric_limits<float>::infinity();
    int distance = get_h(env, agent_location, task.start_location);
    int length = 0;
    for (size_t i = 1; i < task.locations.size(); ++i) {
        length += get_h(env, task.locations[i - 1], task.locations[i]);
    }
    float value = config.dist_weight * static_cast<float>(distance) + static_cast<float>(length);
    if (config.flow_aware_spatial && background_flow.size() == env->map.size()) {
        // This is deliberately a local proxy, not a per-pair Dijkstra: at SL scale
        // it keeps assignment sparse while using the planner's existing flow signal.
        float corridor_density = 0.5f * flow_density(background_flow, agent_location) +
                                 flow_density(background_flow, task.start_location);
        for (size_t i = 1; i < task.locations.size() && i <= 3; ++i) {
            corridor_density += 0.5f * flow_density(background_flow, task.locations[i]);
        }
        value += std::max(0.0f, config.traffic_weight) * corridor_density;
    }
    if (config.opened_flow_penalty_weight > 0.0f) {
        const auto penalty = opened_task_flow_penalties_.find(task.id);
        if (penalty != opened_task_flow_penalties_.end()) {
            value += config.opened_flow_penalty_weight * penalty->second;
        }
    }
    return value;
}

void PortableGreedyHeapScheduler::build_opened_task_flow_penalties(
    SharedEnvironment* env, const std::vector<TaskInfo>& tasks,
    const PortableGreedyHeapConfig& config)
{
    opened_task_flow_penalties_.clear();
    if (config.opened_flow_penalty_weight <= 0.0f || config.opened_flow_zone_rows <= 0 ||
        config.opened_flow_zone_cols <= 0) {
        return;
    }

    OpenedTaskFlow flow(env, config.opened_flow_zone_rows, config.opened_flow_zone_cols);
    for (const auto& entry : env->task_pool) {
        const Task& task = entry.second;
        if (task.idx_next_loc <= 0 || task.agent_assigned < 0 ||
            task.agent_assigned >= static_cast<int>(env->curr_states.size()) ||
            task.idx_next_loc >= static_cast<int>(task.locations.size())) {
            continue;
        }
        flow.add_path(env->curr_states[task.agent_assigned].location,
                      std::vector<int>(task.locations.begin() + task.idx_next_loc, task.locations.end()));
    }
    for (const TaskInfo& task : tasks) {
        opened_task_flow_penalties_.emplace(task.id, flow.path_penalty(task.locations));
    }
}

void PortableGreedyHeapScheduler::rebuild_candidates(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const PortableGreedyHeapConfig& config,
    const std::vector<Double4>& background_flow, int sort_k,
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

    const int cell_size = std::max(1, config.spatial_cell_size);
    const int rows = std::max(1, env->rows);
    const int cols = std::max(1, env->cols);
    const int cell_rows = (rows + cell_size - 1) / cell_size;
    const int cell_cols = (cols + cell_size - 1) / cell_size;
    std::vector<std::vector<int>> task_cells;
    if (config.flow_aware_spatial) {
        task_cells.resize(cell_rows * cell_cols);
        for (int ti = 0; ti < static_cast<int>(tasks.size()); ++ti) {
            const int row = std::clamp(tasks[ti].start_location / cols, 0, rows - 1);
            const int col = std::clamp(tasks[ti].start_location % cols, 0, cols - 1);
            task_cells[(row / cell_size) * cell_cols + col / cell_size].push_back(ti);
        }
    }

    for (size_t ai = 0; ai < agents.size(); ++ai) {
        if (std::chrono::steady_clock::now() >= deadline) break;
        if (!config.flow_aware_spatial && same_task_set && same_shape && candidate_agent_ids_[ai] == agents[ai].id &&
            candidate_agent_locations_[ai] == agents[ai].location && !candidates_[ai].empty()) {
            continue;
        }
        auto& list = candidates_[ai];
        list.clear();
        const int scan_limit = std::min(static_cast<int>(tasks.size()), std::max(
            1, config.flow_aware_spatial ? config.spatial_candidate_limit : static_cast<int>(tasks.size())));
        list.reserve(scan_limit);
        std::vector<int> nearby_tasks;
        if (config.flow_aware_spatial) {
            const int row = std::clamp(agents[ai].location / cols, 0, rows - 1);
            const int col = std::clamp(agents[ai].location % cols, 0, cols - 1);
            const int cell_row = row / cell_size;
            const int cell_col = col / cell_size;
            std::vector<std::pair<int, int>> cells;
            cells.reserve(task_cells.size());
            for (int cell = 0; cell < static_cast<int>(task_cells.size()); ++cell) {
                if (task_cells[cell].empty()) continue;
                const int cr = cell / cell_cols;
                const int cc = cell % cell_cols;
                cells.emplace_back(std::abs(cr - cell_row) + std::abs(cc - cell_col), cell);
            }
            std::sort(cells.begin(), cells.end());
            nearby_tasks.reserve(scan_limit);
            for (const auto& cell : cells) {
                for (const int ti : task_cells[cell.second]) {
                    nearby_tasks.push_back(ti);
                    if (static_cast<int>(nearby_tasks.size()) >= scan_limit) break;
                }
                if (static_cast<int>(nearby_tasks.size()) >= scan_limit) break;
            }
        }
        const int candidate_count = config.flow_aware_spatial ?
            static_cast<int>(nearby_tasks.size()) : static_cast<int>(tasks.size());
        for (int index = 0; index < candidate_count; ++index) {
            if ((index & 31) == 0 && std::chrono::steady_clock::now() >= deadline) break;
            const int ti = config.flow_aware_spatial ? nearby_tasks[index] : index;
            list.push_back({score(env, agents[ai].location, tasks[ti], config, background_flow), ti});
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

std::vector<PortableGreedyHeapScheduler::Match> PortableGreedyHeapScheduler::flow_aware_match(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const PortableGreedyHeapConfig& config,
    const std::vector<Double4>& background_flow,
    std::chrono::steady_clock::time_point deadline) const
{
    struct AgentOrder {
        int index;
        float regret;
        float best_score;
    };

    std::vector<AgentOrder> order;
    order.reserve(agents.size());
    for (int ai = 0; ai < static_cast<int>(candidates_.size()); ++ai) {
        const auto& list = candidates_[ai];
        if (list.empty()) continue;
        const float alternative = list.size() > 1 ? list[1].score : list[0].score + 1.0e6f;
        order.push_back({ai, alternative - list[0].score, list[0].score});
    }
    std::sort(order.begin(), order.end(), [&](const AgentOrder& lhs, const AgentOrder& rhs) {
        if (config.future_flow_regret_order && lhs.regret != rhs.regret) {
            return lhs.regret > rhs.regret;
        }
        return lhs.best_score < rhs.best_score;
    });

    ZoneFutureFlow future_flow(env, background_flow, config.future_flow_cell_size);
    std::vector<bool> used_tasks(tasks.size(), false);
    std::vector<Match> matches;
    matches.reserve(order.size());

    for (const AgentOrder& ordered : order) {
        if (std::chrono::steady_clock::now() >= deadline) break;
        const auto& list = candidates_[ordered.index];
        int chosen = -1;
        float chosen_score = std::numeric_limits<float>::infinity();
        for (const Candidate& candidate : list) {
            if (candidate.task_index < 0 || candidate.task_index >= static_cast<int>(tasks.size()) ||
                used_tasks[candidate.task_index]) {
                continue;
            }
            bool blocked = false;
            const float dynamic_penalty = future_flow.route_penalty(
                agents[ordered.index].location, tasks[candidate.task_index].locations,
                std::max(0.0f, config.future_flow_hard_cap), &blocked);
            if (blocked) continue;
            const float dynamic_score = candidate.score +
                std::max(0.0f, config.future_flow_weight) * dynamic_penalty;
            if (dynamic_score < chosen_score) {
                chosen = candidate.task_index;
                chosen_score = dynamic_score;
            }
        }
        if (chosen < 0) continue;
        used_tasks[chosen] = true;
        matches.emplace_back(agents[ordered.index].id, tasks[chosen].id);
        future_flow.add_route(agents[ordered.index].location, tasks[chosen].locations);
    }
    return matches;
}

void PortableGreedyHeapScheduler::refine_lns(
    SharedEnvironment* env, std::vector<Match>& matches, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const std::unordered_map<int, int>& old_assignment,
    const PortableGreedyHeapConfig& config, const std::vector<Double4>& background_flow,
    std::chrono::steady_clock::time_point deadline) const
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
        float value = score(env, agent_it->second, *task_it->second, config, background_flow);
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
        if (after + std::max(0.0f, config.reassign_min_gain) < before) {
            std::swap(matches[first].second, matches[second].second);
        }
    }
}

void PortableGreedyHeapScheduler::schedule(int time_limit_ms, std::vector<int>& proposed_schedule,
                                           SharedEnvironment* env,
                                           const PortableGreedyHeapConfig& config,
                                           const std::vector<Double4>& background_flow)
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

    // Preserve active and stable assignments, then cap only newly assigned
    // agents. This mirrors the contest GreedyHeap throttling semantics.
    const float max_assign_ratio = std::clamp(config.max_assign_ratio, 0.0f, 1.0f);
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

    std::vector<TaskInfo> scoring_tasks = free_tasks;
    scoring_tasks.insert(scoring_tasks.end(), reassign_tasks.begin(), reassign_tasks.end());
    build_opened_task_flow_penalties(env, scoring_tasks, config);
    if (config.opened_flow_penalty_weight > 0.0f) {
        // Scores depend on the current opened-work snapshot, so old candidates
        // cannot be reused across scheduling cycles.
        candidates_.clear();
        candidate_agent_ids_.clear();
        candidate_agent_locations_.clear();
        candidate_task_ids_.clear();
    }
    rebuild_candidates(env, free_agents, free_tasks, config, background_flow, config.sort_k, rebuild_deadline);
    std::vector<Match> matches = config.flow_aware_spatial && config.future_flow_weight > 0.0f
        ? flow_aware_match(env, free_agents, free_tasks, config, background_flow, lns_deadline)
        : lazy_match(free_agents, free_tasks, lns_deadline);

    std::vector<AgentInfo> refinement_agents = free_agents;
    refinement_agents.insert(refinement_agents.end(), reassign_agents.begin(), reassign_agents.end());
    std::vector<TaskInfo> refinement_tasks = free_tasks;
    refinement_tasks.insert(refinement_tasks.end(), reassign_tasks.begin(), reassign_tasks.end());
    for (const AgentInfo& agent : reassign_agents) {
        matches.emplace_back(agent.id, old_assignment.at(agent.id));
    }
    if (lns_ms > 0) {
        refine_lns(env, matches, refinement_agents, refinement_tasks, old_assignment, config,
                   background_flow, deadline);
    }

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
                                        const PortableGreedyHeapConfig& config,
                                        const std::vector<Double4>& background_flow)
{
    static PortableGreedyHeapScheduler scheduler;
    scheduler.schedule(time_limit_ms, proposed_schedule, env, config, background_flow);
}

}  // namespace DefaultPlanner
