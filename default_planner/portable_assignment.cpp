#include "portable_assignment.h"

#include "heuristics.h"

#include <algorithm>
#include <array>
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
    std::vector<int> locations;
};

// Coarse future-flow is intentionally an assignment-side estimate: it looks
// at service legs already committed in the task pool, without changing MAPF
// routes or reserving any cells.  It matches the successful Sortation Large
// GreedyHeap experiment, but is reusable by TaskMatcher candidate scoring.
class OpenedTaskZoneFlow {
public:
    OpenedTaskZoneFlow(SharedEnvironment* env, const PortableTaskMatcherConfig& config)
        : env_(env), rows_(std::max(0, config.future_flow_zone_rows)),
          cols_(std::max(0, config.future_flow_zone_cols)),
          weight_(std::max(0.0F, config.future_flow_penalty_weight))
    {
        if (!config.future_flow_enabled || weight_ <= 0.0F || rows_ <= 0 || cols_ <= 0 ||
            env_ == nullptr || env_->rows <= 0 || env_->cols <= 0) {
            rows_ = cols_ = 0;
            weight_ = 0.0F;
            return;
        }
        flow_.assign(static_cast<size_t>(rows_ * cols_), {});
        for (const auto& entry : env_->task_pool) {
            const Task& task = entry.second;
            if (task.idx_next_loc <= 0 || task.agent_assigned < 0 ||
                task.agent_assigned >= static_cast<int>(env_->curr_states.size()) ||
                task.idx_next_loc >= static_cast<int>(task.locations.size())) continue;
            int previous = env_->curr_states.at(task.agent_assigned).location;
            for (size_t i = static_cast<size_t>(task.idx_next_loc); i < task.locations.size(); ++i) {
                const int next = task.locations[i];
                add_path(zone_of(previous), zone_of(next));
                previous = next;
            }
        }
    }

    bool enabled() const { return weight_ > 0.0F; }

    float task_penalty(const TaskInfo& task) const
    {
        if (!enabled() || task.locations.size() < 2) return 0.0F;
        float total = 0.0F;
        int segments = 0;
        for (size_t i = 1; i < task.locations.size(); ++i) {
            total += path_penalty(zone_of(task.locations[i - 1]), zone_of(task.locations[i]));
            ++segments;
        }
        return segments > 0 ? weight_ * total / static_cast<float>(segments) : 0.0F;
    }

private:
    int zone_of(int location) const
    {
        if (location < 0 || rows_ <= 0 || cols_ <= 0) return -1;
        const int row = location / env_->cols;
        const int col = location % env_->cols;
        if (row < 0 || row >= env_->rows || col < 0 || col >= env_->cols) return -1;
        return std::min(rows_ - 1, row * rows_ / env_->rows) * cols_ +
               std::min(cols_ - 1, col * cols_ / env_->cols);
    }

    void add_path(int from, int to)
    {
        if (from < 0 || to < 0 || from >= static_cast<int>(flow_.size()) ||
            to >= static_cast<int>(flow_.size())) return;
        int row = from / cols_, col = from % cols_;
        const int target_row = to / cols_, target_col = to % cols_;
        while (row != target_row) {
            const int direction = target_row > row ? 1 : 3;
            flow_[row * cols_ + col][direction] += 1.0F;
            row += target_row > row ? 1 : -1;
        }
        while (col != target_col) {
            const int direction = target_col > col ? 0 : 2;
            flow_[row * cols_ + col][direction] += 1.0F;
            col += target_col > col ? 1 : -1;
        }
    }

    float path_penalty(int from, int to) const
    {
        if (from < 0 || to < 0 || from >= static_cast<int>(flow_.size()) ||
            to >= static_cast<int>(flow_.size())) return 0.0F;
        int row = from / cols_, col = from % cols_;
        const int target_row = to / cols_, target_col = to % cols_;
        float total = 0.0F;
        int steps = 0;
        const auto add_step = [&](int direction, int next_row, int next_col) {
            const int zone = row * cols_ + col;
            const int next_zone = next_row * cols_ + next_col;
            const int opposite = (direction + 2) % 4;
            const float arrival = flow_[next_zone][0] + flow_[next_zone][1] +
                                  flow_[next_zone][2] + flow_[next_zone][3];
            total += flow_[zone][direction] + flow_[next_zone][opposite] + 0.25F * arrival;
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
        return steps > 0 ? total / static_cast<float>(steps) : 0.0F;
    }

    SharedEnvironment* env_;
    int rows_;
    int cols_;
    float weight_;
    std::vector<std::array<float, 4>> flow_;
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

float flow_traffic_edge_cost(SharedEnvironment* env, const std::vector<Double4>& background_flow,
                             int location, int next, float congestion_weight)
{
    if (background_flow.size() != env->map.size()) return 1;
    const int direction = get_d(location - next, env);
    const int contraflow = (background_flow[location].d[direction] + 1) *
                           background_flow[next].d[(direction + 2) % 4];
    int incoming = 0;
    for (int d = 0; d < 4; ++d) incoming += background_flow[next].d[d];
    return 1.0F + std::max(0.0F, congestion_weight) *
        static_cast<float>(contraflow + incoming / 2);
}

float task_service_congestion_proxy(SharedEnvironment* env, const TaskInfo& task,
                                    const std::vector<Double4>& background_flow)
{
    if (background_flow.size() != env->map.size() || task.locations.size() < 2) return 0.0F;
    float penalty = 0.0F;
    for (size_t i = 1; i < task.locations.size(); ++i) {
        const int segment = get_h(env, task.locations[i - 1], task.locations[i]);
        if (segment <= 0 || segment >= std::numeric_limits<int>::max() / 8) continue;
        float endpoint_flow = 0.0F;
        for (int direction = 0; direction < 4; ++direction) {
            endpoint_flow += static_cast<float>(background_flow[task.locations[i - 1]].d[direction]);
            endpoint_flow += static_cast<float>(background_flow[task.locations[i]].d[direction]);
        }
        // A delivery leg through busy endpoints is more likely to create the
        // loaded waiting seen in the W500 diagnostics.  This is deliberately
        // a cheap proxy: unlike a second Dijkstra per task, it stays usable
        // under the 1 s online scheduling budget.
        penalty += static_cast<float>(segment) * endpoint_flow * 0.5F;
    }
    return penalty;
}

std::vector<std::vector<float>> traffic_cost_matrix(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const PortableTaskMatcherConfig& config,
    const std::vector<Double4>& background_flow, std::chrono::steady_clock::time_point deadline)
{
    std::vector<std::vector<float>> cost(agents.size(),
                                         std::vector<float>(tasks.size(), kInvalidCost));
    const int keep = std::min(std::max(1, config.traffic_top_k), static_cast<int>(tasks.size()));
    std::vector<int> lengths(tasks.size(), -2);
    std::vector<float> service_penalties(tasks.size(), -1.0F);

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
                        if (service_penalties[task_index] < 0.0F)
                            service_penalties[task_index] = task_service_congestion_proxy(
                                env, tasks[task_index], background_flow);
                        cost[i][task_index] = config.dist_weight * static_cast<float>(current_cost) +
                                              static_cast<float>(lengths[task_index]) +
                                              std::max(0.0F, config.traffic_service_weight) *
                                                  service_penalties[task_index];
                    }
                }
                task_indices_at_location.erase(goal);
                --remaining_goals;
            }
            for (const int next : global_neighbors.at(location)) {
                const float next_cost = current_cost + flow_traffic_edge_cost(
                    env, background_flow, location, next, config.traffic_congestion_weight);
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

// A bounded alternative to top_k_match for very large teams.  The original
// fallback first scans every task for every agent, which consumes the complete
// online scheduling budget before emitting a single assignment on SL-8000.
// Here tasks are indexed by small map cells.  An agent examines only the
// closest nonempty cells, and each cell advances a cursor as tasks are taken.
// This intentionally trades exact global matching for predictable progress.
std::vector<std::pair<int, int>> spatial_bounded_match(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const PortableTaskMatcherConfig& config,
    const std::vector<Double4>& background_flow, std::chrono::steady_clock::time_point deadline)
{
    const int side = std::max(1, config.scalable_bucket_size);
    const int bucket_cols = std::max(1, (env->cols + side - 1) / side);
    const int bucket_rows = std::max(1, (env->rows + side - 1) / side);
    const auto bucket_of = [&](int location) {
        const int row = location / env->cols;
        const int col = location % env->cols;
        return (row / side) * bucket_cols + (col / side);
    };

    std::vector<std::vector<int>> buckets(bucket_rows * bucket_cols);
    for (int task_index = 0; task_index < static_cast<int>(tasks.size()); ++task_index) {
        buckets[bucket_of(tasks[task_index].pickup)].push_back(task_index);
    }
    std::vector<size_t> next_in_bucket(buckets.size(), 0);
    // Forecast newly assigned pickup pressure within this scheduling round.
    std::vector<int> predicted_pickup_load(buckets.size(), 0);
    std::vector<int> lengths(tasks.size(), -2);
    std::vector<float> service_penalties(tasks.size(), -1.0F);
    OpenedTaskZoneFlow future_flow(env, config);
    std::vector<float> future_penalties(tasks.size(), -1.0F);
    std::vector<std::pair<int, int>> result;
    result.reserve(agents.size());
    const int per_bucket = std::max(1, config.scalable_candidates_per_bucket);
    const bool zone_capacity = config.scalable_zone_capacity;
    const int capacity_limit = std::max(0, config.scalable_zone_capacity_limit);
    const float capacity_weight = std::max(0.0F, config.scalable_zone_capacity_weight);
    const int capacity_radius = std::max(0, config.scalable_zone_capacity_radius);

    for (const AgentInfo& agent : agents) {
        if (std::chrono::steady_clock::now() >= deadline) break;
        const int agent_row = agent.location / env->cols / side;
        const int agent_col = agent.location % env->cols / side;
        float best_score = kInvalidCost;
        int best_bucket = -1;
        int best_task = -1;

        // The first ring that exposes any candidate is sufficient: farther
        // rings cannot beat its cell-level Manhattan lower bound.  Looking at
        // every cell on that ring prevents a fixed row-major bias.
        const int max_ring = std::max(bucket_rows, bucket_cols);
        for (int ring = 0; ring <= max_ring; ++ring) {
            for (int dr = -ring; dr <= ring; ++dr) {
                for (int dc = -ring; dc <= ring; ++dc) {
                    if (std::max(std::abs(dr), std::abs(dc)) != ring) continue;
                    const int row = agent_row + dr;
                    const int col = agent_col + dc;
                    if (row < 0 || row >= bucket_rows || col < 0 || col >= bucket_cols) continue;
                    const int bucket = row * bucket_cols + col;
                    const auto& entries = buckets[bucket];
                    const size_t first = next_in_bucket[bucket];
                    for (size_t offset = 0; offset < static_cast<size_t>(per_bucket) &&
                                            first + offset < entries.size(); ++offset) {
                        const int task_index = entries[first + offset];
                        if (lengths[task_index] == -2)
                            lengths[task_index] = task_length(env, tasks[task_index]);
                        if (lengths[task_index] < 0) continue;
                        const int distance = get_h(env, agent.location, tasks[task_index].pickup);
                        if (distance >= std::numeric_limits<int>::max() / 8) continue;
                        float score = config.dist_weight * static_cast<float>(distance) +
                                      static_cast<float>(lengths[task_index]);
                        if (config.use_traffic_cost && background_flow.size() == env->map.size()) {
                            float pickup_flow = 0.0F;
                            for (int direction = 0; direction < 4; ++direction)
                                pickup_flow += static_cast<float>(background_flow[tasks[task_index].pickup].d[direction]);
                            if (service_penalties[task_index] < 0.0F)
                                service_penalties[task_index] = task_service_congestion_proxy(
                                    env, tasks[task_index], background_flow);
                            score += std::max(0.0F, config.traffic_congestion_weight) * pickup_flow;
                            score += std::max(0.0F, config.traffic_service_weight) *
                                     service_penalties[task_index];
                        }
                        if (zone_capacity) {
                            const int overflow = std::max(
                                0, predicted_pickup_load[bucket] - capacity_limit + 1);
                            score += capacity_weight * static_cast<float>(overflow * overflow);
                        }
                        if (future_flow.enabled()) {
                            if (future_penalties[task_index] < 0.0F)
                                future_penalties[task_index] = future_flow.task_penalty(tasks[task_index]);
                            score += future_penalties[task_index];
                        }
                        if (score < best_score) {
                            best_score = score;
                            best_bucket = bucket;
                            best_task = task_index;
                        }
                    }
                }
            }
            if (best_task >= 0 && (!zone_capacity || ring >= capacity_radius)) break;
        }
        if (best_task >= 0) {
            // Consume every earlier task in the selected cell as well: it was
            // considered by this agent and keeping it would make later scans
            // revisit the same entries indefinitely.
            const auto& entries = buckets[best_bucket];
            while (next_in_bucket[best_bucket] < entries.size() &&
                   entries[next_in_bucket[best_bucket]] != best_task) {
                ++next_in_bucket[best_bucket];
            }
            if (next_in_bucket[best_bucket] < entries.size()) ++next_in_bucket[best_bucket];
            ++predicted_pickup_load[best_bucket];
            result.emplace_back(agent.id, tasks[best_task].id);
        }
    }
    return result;
}

// Sparse counterpart of the GreedyHeap pattern: spatial indexing bounds the
// candidate generation work, while a single global heap removes the
// agent-iteration bias of spatial_bounded_match.  This deliberately stores
// only local candidates, so it remains practical when A*T is too large for a
// dense cost matrix.
std::vector<std::pair<int, int>> sparse_global_heap_match(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const PortableTaskMatcherConfig& config,
    const std::vector<Double4>& background_flow, std::chrono::steady_clock::time_point deadline,
    int max_matches = std::numeric_limits<int>::max())
{
    struct Edge {
        float score;
        int agent;
        int task;
        bool operator>(const Edge& other) const {
            if (score != other.score) return score > other.score;
            if (agent != other.agent) return agent > other.agent;
            return task > other.task;
        }
    };
    const int side = std::max(1, config.scalable_bucket_size);
    const int bucket_cols = std::max(1, (env->cols + side - 1) / side);
    const int bucket_rows = std::max(1, (env->rows + side - 1) / side);
    const auto bucket_of = [&](int location) {
        const int row = location / env->cols;
        const int col = location % env->cols;
        return (row / side) * bucket_cols + (col / side);
    };

    std::vector<std::vector<int>> buckets(bucket_rows * bucket_cols);
    for (int task = 0; task < static_cast<int>(tasks.size()); ++task)
        buckets[bucket_of(tasks[task].pickup)].push_back(task);

    std::vector<int> lengths(tasks.size(), -2);
    std::vector<float> service_penalties(tasks.size(), -1.0F);
    std::priority_queue<Edge, std::vector<Edge>, std::greater<Edge>> heap;
    const int per_bucket = std::max(1, config.scalable_candidates_per_bucket);
    const int per_agent = std::max(1, config.scalable_global_candidates);
    const int max_ring = std::max(bucket_rows, bucket_cols);

    for (int ai = 0; ai < static_cast<int>(agents.size()); ++ai) {
        if (std::chrono::steady_clock::now() >= deadline) break;
        const int base_row = agents[ai].location / env->cols / side;
        const int base_col = agents[ai].location % env->cols / side;
        int added = 0;
        for (int ring = 0; ring <= max_ring && added < per_agent; ++ring) {
            for (int dr = -ring; dr <= ring && added < per_agent; ++dr) {
                for (int dc = -ring; dc <= ring && added < per_agent; ++dc) {
                    if (std::max(std::abs(dr), std::abs(dc)) != ring) continue;
                    const int row = base_row + dr;
                    const int col = base_col + dc;
                    if (row < 0 || row >= bucket_rows || col < 0 || col >= bucket_cols) continue;
                    const auto& entries = buckets[row * bucket_cols + col];
                    for (int task : entries) {
                        if (added >= per_agent) break;
                        if (lengths[task] == -2) lengths[task] = task_length(env, tasks[task]);
                        if (lengths[task] < 0) continue;
                        const int distance = get_h(env, agents[ai].location, tasks[task].pickup);
                        if (distance >= std::numeric_limits<int>::max() / 8) continue;
                        float score = config.dist_weight * static_cast<float>(distance) +
                                      static_cast<float>(lengths[task]);
                        if (config.use_traffic_cost && background_flow.size() == env->map.size()) {
                            float pickup_flow = 0.0F;
                            for (int direction = 0; direction < 4; ++direction)
                                pickup_flow += static_cast<float>(background_flow[tasks[task].pickup].d[direction]);
                            if (service_penalties[task] < 0.0F)
                                service_penalties[task] = task_service_congestion_proxy(
                                    env, tasks[task], background_flow);
                            score += std::max(0.0F, config.traffic_congestion_weight) * pickup_flow;
                            score += std::max(0.0F, config.traffic_service_weight) * service_penalties[task];
                        }
                        heap.push({score, ai, task});
                        ++added;
                        if (added % per_bucket == 0) break;
                    }
                }
            }
        }
    }

    std::vector<unsigned char> agent_used(agents.size(), 0);
    std::vector<unsigned char> task_used(tasks.size(), 0);
    std::vector<std::pair<int, int>> result;
    result.reserve(agents.size());
    while (!heap.empty() && static_cast<int>(result.size()) < max_matches &&
           std::chrono::steady_clock::now() < deadline) {
        const Edge edge = heap.top();
        heap.pop();
        if (agent_used[edge.agent] || task_used[edge.task]) continue;
        agent_used[edge.agent] = 1;
        task_used[edge.task] = 1;
        result.emplace_back(agents[edge.agent].id, tasks[edge.task].id);
    }
    return result;
}

// Assignment-only subset of HardWarehouse: connected-component filtering plus
// a zone-bounded, bilateral sparse pair graph.  It intentionally does not
// alter goals, paths, or the planner; the existing traffic score remains the
// only congestion signal in this portable scheduler.
std::vector<std::pair<int, int>> bilateral_zone_heap_match(
    SharedEnvironment* env, const std::vector<AgentInfo>& agents,
    const std::vector<TaskInfo>& tasks, const PortableTaskMatcherConfig& config,
    const std::vector<Double4>& background_flow, std::chrono::steady_clock::time_point deadline,
    int max_matches = std::numeric_limits<int>::max())
{
    struct Edge {
        float score;
        int agent;
        int task;
        bool operator>(const Edge& other) const {
            if (score != other.score) return score > other.score;
            if (agent != other.agent) return agent > other.agent;
            return task > other.task;
        }
    };
    struct ComponentCache {
        SharedEnvironment* env = nullptr;
        int size = 0;
        std::vector<int> ids;
    };
    static ComponentCache component_cache;
    const int map_size = static_cast<int>(env->map.size());
    if (component_cache.env != env || component_cache.size != map_size) {
        component_cache.env = env;
        component_cache.size = map_size;
        component_cache.ids.assign(map_size, -1);
        int component = 0;
        const int delta[4] = {-env->cols, env->cols, -1, 1};
        for (int start = 0; start < map_size; ++start) {
            if (env->map[start] != 0 || component_cache.ids[start] >= 0) continue;
            std::vector<int> queue{start};
            component_cache.ids[start] = component;
            for (size_t head = 0; head < queue.size(); ++head) {
                const int location = queue[head];
                const int col = location % env->cols;
                for (int direction = 0; direction < 4; ++direction) {
                    if ((direction == 2 && col == 0) ||
                        (direction == 3 && col == env->cols - 1)) continue;
                    const int next = location + delta[direction];
                    if (next < 0 || next >= map_size || env->map[next] != 0 ||
                        component_cache.ids[next] >= 0) continue;
                    component_cache.ids[next] = component;
                    queue.push_back(next);
                }
            }
            ++component;
        }
    }

    const int side = std::max(1, config.scalable_bucket_size);
    const int bucket_cols = std::max(1, (env->cols + side - 1) / side);
    const int bucket_rows = std::max(1, (env->rows + side - 1) / side);
    const auto bucket_of = [&](int location) {
        const int row = location / env->cols;
        const int col = location % env->cols;
        return (row / side) * bucket_cols + (col / side);
    };
    std::vector<std::vector<int>> task_buckets(bucket_rows * bucket_cols);
    std::vector<std::vector<int>> agent_buckets(bucket_rows * bucket_cols);
    for (int task = 0; task < static_cast<int>(tasks.size()); ++task)
        task_buckets[bucket_of(tasks[task].pickup)].push_back(task);
    for (int agent = 0; agent < static_cast<int>(agents.size()); ++agent)
        agent_buckets[bucket_of(agents[agent].location)].push_back(agent);

    std::vector<int> lengths(tasks.size(), -2);
    std::vector<float> service_penalties(tasks.size(), -1.0F);
    std::priority_queue<Edge, std::vector<Edge>, std::greater<Edge>> heap;
    std::unordered_set<unsigned long long> edge_seen;
    edge_seen.reserve(static_cast<size_t>(agents.size()) *
                      static_cast<size_t>(std::max(1, config.scalable_global_candidates)));
    const int per_agent = std::max(1, config.scalable_global_candidates);
    const int per_task = std::max(1, config.scalable_task_candidates);
    const int max_ring = std::min(std::max(bucket_rows, bucket_cols),
                                  std::max(0, config.scalable_zone_radius));

    const auto same_component = [&](int agent_location, int task_location) {
        return agent_location >= 0 && agent_location < map_size &&
               task_location >= 0 && task_location < map_size &&
               component_cache.ids[agent_location] >= 0 &&
               component_cache.ids[agent_location] == component_cache.ids[task_location];
    };
    const auto add_edge = [&](int ai, int ti) {
        if (!same_component(agents[ai].location, tasks[ti].pickup)) return;
        const unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(ai)) << 32) |
                                       static_cast<unsigned int>(ti);
        if (!edge_seen.insert(key).second) return;
        if (lengths[ti] == -2) lengths[ti] = task_length(env, tasks[ti]);
        if (lengths[ti] < 0) return;
        const int distance = get_h(env, agents[ai].location, tasks[ti].pickup);
        if (distance >= std::numeric_limits<int>::max() / 8) return;
        float score = config.dist_weight * static_cast<float>(distance) +
                      static_cast<float>(lengths[ti]);
        if (config.use_traffic_cost && background_flow.size() == env->map.size()) {
            float pickup_flow = 0.0F;
            for (int direction = 0; direction < 4; ++direction)
                pickup_flow += static_cast<float>(background_flow[tasks[ti].pickup].d[direction]);
            if (service_penalties[ti] < 0.0F)
                service_penalties[ti] = task_service_congestion_proxy(env, tasks[ti], background_flow);
            score += std::max(0.0F, config.traffic_congestion_weight) * pickup_flow;
            score += std::max(0.0F, config.traffic_service_weight) * service_penalties[ti];
        }
        heap.push({score, ai, ti});
    };

    const auto scan_task_neighbors = [&](int ai, int limit) {
        const int base_row = agents[ai].location / env->cols / side;
        const int base_col = agents[ai].location % env->cols / side;
        int added = 0;
        for (int ring = 0; ring <= max_ring && added < limit; ++ring) {
            for (int dr = -ring; dr <= ring && added < limit; ++dr) {
                for (int dc = -ring; dc <= ring && added < limit; ++dc) {
                    if (std::max(std::abs(dr), std::abs(dc)) != ring) continue;
                    const int row = base_row + dr, col = base_col + dc;
                    if (row < 0 || row >= bucket_rows || col < 0 || col >= bucket_cols) continue;
                    for (int ti : task_buckets[row * bucket_cols + col]) {
                        if (added >= limit) break;
                        const size_t before = edge_seen.size();
                        add_edge(ai, ti);
                        if (edge_seen.size() != before) ++added;
                    }
                }
            }
        }
    };
    const auto scan_agent_neighbors = [&](int ti, int limit) {
        const int base_row = tasks[ti].pickup / env->cols / side;
        const int base_col = tasks[ti].pickup % env->cols / side;
        int added = 0;
        for (int ring = 0; ring <= max_ring && added < limit; ++ring) {
            for (int dr = -ring; dr <= ring && added < limit; ++dr) {
                for (int dc = -ring; dc <= ring && added < limit; ++dc) {
                    if (std::max(std::abs(dr), std::abs(dc)) != ring) continue;
                    const int row = base_row + dr, col = base_col + dc;
                    if (row < 0 || row >= bucket_rows || col < 0 || col >= bucket_cols) continue;
                    for (int ai : agent_buckets[row * bucket_cols + col]) {
                        if (added >= limit) break;
                        const size_t before = edge_seen.size();
                        add_edge(ai, ti);
                        if (edge_seen.size() != before) ++added;
                    }
                }
            }
        }
    };
    for (int ai = 0; ai < static_cast<int>(agents.size()) && std::chrono::steady_clock::now() < deadline; ++ai)
        scan_task_neighbors(ai, per_agent);
    for (int ti = 0; ti < static_cast<int>(tasks.size()) && std::chrono::steady_clock::now() < deadline; ++ti)
        scan_agent_neighbors(ti, per_task);

    std::vector<unsigned char> agent_used(agents.size(), 0), task_used(tasks.size(), 0);
    std::vector<std::pair<int, int>> result;
    result.reserve(std::min(agents.size(), tasks.size()));
    while (!heap.empty() && static_cast<int>(result.size()) < max_matches &&
           std::chrono::steady_clock::now() < deadline) {
        const Edge edge = heap.top(); heap.pop();
        if (agent_used[edge.agent] || task_used[edge.task]) continue;
        agent_used[edge.agent] = 1;
        task_used[edge.task] = 1;
        result.emplace_back(agents[edge.agent].id, tasks[edge.task].id);
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
    const float max_assign_ratio = std::clamp(config.max_assign_ratio, 0.0F, 1.0F);
    const int currently_assigned = static_cast<int>(std::count_if(
        local.begin(), local.end(), [](int task_id) { return task_id >= 0; }));
    const int max_total = std::max(1, static_cast<int>(max_assign_ratio * env->num_of_agents));
    const int max_new = max_assign_ratio < 0.999F
        ? std::max(0, max_total - currently_assigned)
        : env->num_of_agents;
    int admitted_free_agents = 0;

    for (int agent = 0; agent < env->num_of_agents; ++agent) {
        const int task_id = local[agent];
        if (task_id < 0) {
            if (admitted_free_agents < max_new) {
                candidates.push_back({agent, env->curr_states.at(agent).location});
                ++admitted_free_agents;
            }
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
                    cost[i][j] = task_score(env, candidates[i].location, tasks[j], config.dist_weight);
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
        if (config.scalable_mode) {
            if (!config.scalable_global_heap) {
                matches = spatial_bounded_match(env, candidates, tasks, config,
                                                background_flow, deadline);
            } else {
                const float fraction = std::clamp(config.scalable_global_heap_fraction, 0.0F, 1.0F);
                const int global_limit = static_cast<int>(std::ceil(
                    fraction * static_cast<float>(std::min(candidates.size(), tasks.size()))));
                matches = config.scalable_bilateral_heap
                    ? bilateral_zone_heap_match(env, candidates, tasks, config, background_flow,
                                                deadline, global_limit)
                    : sparse_global_heap_match(env, candidates, tasks, config, background_flow,
                                               deadline, global_limit);

                // A partial global phase should improve the best pairs, not
                // leave all collided sparse candidates unmatched.  Complete
                // the remaining coverage with the established bounded mode.
                if (static_cast<int>(matches.size()) < std::min(candidates.size(), tasks.size()) &&
                    std::chrono::steady_clock::now() < deadline) {
                    std::unordered_set<int> used_agents;
                    std::unordered_set<int> used_tasks;
                    for (const auto& match : matches) {
                        used_agents.insert(match.first);
                        used_tasks.insert(match.second);
                    }
                    std::vector<AgentInfo> remaining_agents;
                    std::vector<TaskInfo> remaining_tasks;
                    remaining_agents.reserve(candidates.size() - used_agents.size());
                    remaining_tasks.reserve(tasks.size() - used_tasks.size());
                    for (const AgentInfo& agent : candidates)
                        if (!used_agents.count(agent.id)) remaining_agents.push_back(agent);
                    for (const TaskInfo& task : tasks)
                        if (!used_tasks.count(task.id)) remaining_tasks.push_back(task);
                    std::vector<std::pair<int, int>> fill = spatial_bounded_match(
                        env, remaining_agents, remaining_tasks, config, background_flow, deadline);
                    matches.insert(matches.end(), fill.begin(), fill.end());
                }
            }
        } else {
            matches = top_k_match(env, candidates, tasks, config.dist_weight, 1.0F,
                                  config.candidate_top_k, deadline);
        }
    }
    if (matches.empty()) {
        // A scheduler timeout must not discard a still-valid unopened task.
        for (const auto& entry : old_assignment) local[entry.first] = entry.second;
    } else {
        const int max_assignments = std::max(
            1, static_cast<int>(std::ceil(std::clamp(config.max_assign_ratio, 0.0F, 1.0F) *
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
                float value = task_score(env, agent_locations.at(match.first),
                                         *tasks_by_id.at(match.second), config.dist_weight);
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
