#pragma once

// Deterministic, scheduler-independent tail assignment. Each task must be
// covered exactly once; spare agents may remain idle. Negative pickup entries
// denote unreachable pairs. The objective is lexicographic:
//   (max(fixed_tail, max_a(pickup[a,j] + service[j])), sum_a pickup[a,j]).
// A cancellation never publishes a partial assignment.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace DefaultPlanner {
namespace TailBottleneck {

using Cost = std::int64_t;
using Matrix = std::vector<std::vector<Cost>>;
constexpr Cost kInfinity = std::numeric_limits<Cost>::max() / 4;

enum class Status { optimal, infeasible, cancelled, invalid_input };

struct Objective {
    bool complete = false;
    Cost tail = 0;
    Cost flexible_tail = 0;
    Cost pickup_sum = 0;
};

struct Result {
    Status status = Status::invalid_input;
    std::vector<int> assignment; // agent index -> task index, or -1
    Objective objective;
};

inline bool valid_input(const Matrix& pickup, const std::vector<Cost>& service,
                        Cost fixed_tail)
{
    if (fixed_tail < 0 || fixed_tail >= kInfinity / 2) return false;
    const Cost limit = kInfinity / static_cast<Cost>(service.size() + 2);
    for (const Cost length : service)
        if (length < 0 || length >= limit) return false;
    for (const auto& row : pickup) {
        if (row.size() != service.size()) return false;
        for (const Cost distance : row)
            if (distance >= limit) return false;
    }
    return true;
}

inline Objective evaluate(const Matrix& pickup, const std::vector<Cost>& service,
                          Cost fixed_tail, const std::vector<int>& assignment)
{
    Objective result;
    if (!valid_input(pickup, service, fixed_tail) || assignment.size() != pickup.size())
        return result;
    std::vector<bool> covered(service.size(), false);
    result.tail = fixed_tail;
    for (std::size_t agent = 0; agent < assignment.size(); ++agent) {
        const int task = assignment[agent];
        if (task == -1) continue;
        if (task < 0 || task >= static_cast<int>(service.size()) || covered[task] ||
            pickup[agent][task] < 0)
            return Objective{};
        covered[task] = true;
        result.pickup_sum += pickup[agent][task];
        result.flexible_tail = std::max(result.flexible_tail, pickup[agent][task] + service[task]);
    }
    result.tail = std::max(result.tail, result.flexible_tail);
    result.complete = std::all_of(covered.begin(), covered.end(), [](bool item) { return item; });
    return result;
}

inline bool strictly_better(const Objective& candidate, const Objective& baseline)
{
    return candidate.complete && baseline.complete &&
        (candidate.tail < baseline.tail ||
         (candidate.tail == baseline.tail && candidate.pickup_sum < baseline.pickup_sum));
}

namespace detail {

// Task-side augmenting paths suffice here: |tasks| <= |agents|. Fixed traversal
// order makes the answer independent of hash-table iteration or PRNG state.
template<class Cancel>
bool threshold_augment(int task, const Matrix& pickup, const std::vector<Cost>& service,
                       Cost threshold, std::vector<int>& owner, std::vector<bool>& seen,
                       Cancel& cancelled, bool& aborted)
{
    if (cancelled()) { aborted = true; return false; }
    for (int agent = 0; agent < static_cast<int>(pickup.size()); ++agent) {
        if (cancelled()) { aborted = true; return false; }
        if (seen[agent] || pickup[agent][task] < 0 ||
            pickup[agent][task] + service[task] > threshold) continue;
        seen[agent] = true;
        if (owner[agent] < 0 || threshold_augment(owner[agent], pickup, service,
                                               threshold, owner, seen, cancelled, aborted)) {
            owner[agent] = task;
            return true;
        }
        if (aborted) return false;
    }
    return false;
}

template<class Cancel>
Status feasible(const Matrix& pickup, const std::vector<Cost>& service,
                Cost threshold, Cancel& cancelled)
{
    std::vector<int> owner(pickup.size(), -1);
    for (int task = 0; task < static_cast<int>(service.size()); ++task) {
        std::vector<bool> seen(pickup.size(), false);
        bool aborted = false;
        if (!threshold_augment(task, pickup, service, threshold, owner, seen, cancelled, aborted))
            return aborted ? Status::cancelled : Status::infeasible;
    }
    return Status::optimal;
}

// Rectangular Hungarian, rows=tasks, columns=agents. Forbidden pairs are
// skipped, not replaced by an arbitrary finite penalty. The bottleneck
// feasibility pass guarantees a full matching unless cancellation occurs.
template<class Cancel>
Result minimum_pickup(const Matrix& pickup, const std::vector<Cost>& service,
                      Cost fixed_tail, Cost threshold, Cancel& cancelled)
{
    Result result;
    const int tasks = static_cast<int>(service.size());
    const int agents = static_cast<int>(pickup.size());
    std::vector<Cost> u(tasks + 1), v(agents + 1);
    std::vector<int> p(agents + 1), way(agents + 1);
    for (int task = 1; task <= tasks; ++task) {
        p[0] = task;
        int column = 0;
        std::vector<Cost> minimum(agents + 1, kInfinity);
        std::vector<bool> used(agents + 1, false);
        do {
            if (cancelled()) { result.status = Status::cancelled; return result; }
            used[column] = true;
            const int row = p[column];
            Cost delta = kInfinity;
            int next = 0;
            for (int agent = 1; agent <= agents; ++agent) {
                if (cancelled()) { result.status = Status::cancelled; return result; }
                if (used[agent]) continue;
                const Cost distance = pickup[agent - 1][row - 1];
                if (distance >= 0 && distance + service[row - 1] <= threshold) {
                    const Cost reduced = distance - u[row] - v[agent];
                    if (reduced < minimum[agent]) {
                        minimum[agent] = reduced;
                        way[agent] = column;
                    }
                }
                if (minimum[agent] < delta) {
                    delta = minimum[agent];
                    next = agent;
                }
            }
            if (delta == kInfinity) { result.status = Status::infeasible; return result; }
            for (int agent = 0; agent <= agents; ++agent) {
                if (used[agent]) { u[p[agent]] += delta; v[agent] -= delta; }
                else if (minimum[agent] != kInfinity) minimum[agent] -= delta;
            }
            column = next;
        } while (p[column] != 0);
        do {
            const int previous = way[column];
            p[column] = p[previous];
            column = previous;
        } while (column != 0);
    }
    if (cancelled()) { result.status = Status::cancelled; return result; }
    result.assignment.assign(agents, -1);
    for (int agent = 1; agent <= agents; ++agent)
        if (p[agent] > 0) result.assignment[agent - 1] = p[agent] - 1;
    result.objective = evaluate(pickup, service, fixed_tail, result.assignment);
    result.status = result.objective.complete ? Status::optimal : Status::infeasible;
    return result;
}

} // namespace detail

template<class Cancel>
Result solve(const Matrix& pickup, const std::vector<Cost>& service, Cost fixed_tail,
             Cancel cancelled)
{
    Result result;
    if (cancelled()) { result.status = Status::cancelled; return result; }
    if (!valid_input(pickup, service, fixed_tail)) return result;
    if (service.size() > pickup.size()) { result.status = Status::infeasible; return result; }
    if (service.empty()) {
        result.assignment.assign(pickup.size(), -1);
        result.objective = evaluate(pickup, service, fixed_tail, result.assignment);
        result.status = Status::optimal;
        return result;
    }
    std::vector<Cost> thresholds;
    for (const auto& row : pickup) {
        for (int task = 0; task < static_cast<int>(service.size()); ++task) {
            if (cancelled()) { result.status = Status::cancelled; return result; }
            if (row[task] >= 0)
                thresholds.push_back(std::max(fixed_tail, row[task] + service[task]));
        }
    }
    if (thresholds.empty()) { result.status = Status::infeasible; return result; }
    std::sort(thresholds.begin(), thresholds.end());
    thresholds.erase(std::unique(thresholds.begin(), thresholds.end()), thresholds.end());
    const Status full = detail::feasible(pickup, service, thresholds.back(), cancelled);
    if (full != Status::optimal) { result.status = full; return result; }
    std::size_t low = 0, high = thresholds.size() - 1;
    while (low < high) {
        const std::size_t mid = low + (high - low) / 2;
        const Status status = detail::feasible(pickup, service, thresholds[mid], cancelled);
        if (status == Status::cancelled) { result.status = status; return result; }
        if (status == Status::optimal) high = mid;
        else low = mid + 1;
    }
    return detail::minimum_pickup(pickup, service, fixed_tail, thresholds[low], cancelled);
}

struct Refinement {
    Status status = Status::invalid_input;
    bool accepted = false;
    Objective baseline;
    Objective candidate;
    std::vector<int> assignment; // always baseline unless fully solved and improved
};

template<class Cancel>
Refinement refine(const Matrix& pickup, const std::vector<Cost>& service,
                  Cost fixed_tail, const std::vector<int>& baseline, Cancel cancelled)
{
    Refinement result;
    result.assignment = baseline;
    if (cancelled()) { result.status = Status::cancelled; return result; }
    result.baseline = evaluate(pickup, service, fixed_tail, baseline);
    if (!result.baseline.complete) return result;
    const Result candidate = solve(pickup, service, fixed_tail, cancelled);
    result.status = candidate.status;
    if (candidate.status != Status::optimal) return result;
    if (cancelled()) { result.status = Status::cancelled; return result; }
    result.candidate = candidate.objective;
    if (strictly_better(candidate.objective, result.baseline)) {
        result.assignment = candidate.assignment;
        result.accepted = true;
    }
    return result;
}

} // namespace TailBottleneck
} // namespace DefaultPlanner
