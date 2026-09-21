#include "../default_planner/tail_bottleneck.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace tb = DefaultPlanner::TailBottleneck;

void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

const auto never_cancel = [] { return false; };

struct BruteResult {
    bool feasible = false;
    tb::Cost tail = 0;
    tb::Cost pickup = 0;
};

BruteResult brute_force(const tb::Matrix& matrix, const std::vector<tb::Cost>& service,
                        tb::Cost fixed_tail)
{
    BruteResult best;
    std::vector<bool> used(matrix.size(), false);
    std::function<void(std::size_t, tb::Cost, tb::Cost)> visit =
        [&](std::size_t task, tb::Cost tail, tb::Cost pickup) {
            if (task == service.size()) {
                if (!best.feasible || std::make_pair(tail, pickup) < std::make_pair(best.tail, best.pickup))
                    best = {true, tail, pickup};
                return;
            }
            for (std::size_t agent = 0; agent < matrix.size(); ++agent) {
                if (used[agent] || matrix[agent][task] < 0) continue;
                used[agent] = true;
                visit(task + 1, std::max(tail, matrix[agent][task] + service[task]),
                      pickup + matrix[agent][task]);
                used[agent] = false;
            }
        };
    visit(0, fixed_tail, 0);
    return best;
}

void test_column_constant_counterexample()
{
    const tb::Matrix matrix{{0, 3}, {4, 4}};
    const std::vector<tb::Cost> service{0, 10};
    const std::vector<int> baseline{0, 1};
    // Any linear service coefficient cancels when both tasks are covered.
    for (const double weight : {-1.0, -0.5, 0.0, 0.5, 1.0, 10.0}) {
        const double direct = 0 + 4 + weight * (0 + 10);
        const double swapped = 3 + 4 + weight * (10 + 0);
        require(direct < swapped, "column constant changed sum-minimizing assignment");
    }
    const auto result = tb::refine(matrix, service, 0, baseline, never_cancel);
    require(result.status == tb::Status::optimal && result.accepted, "tail improvement rejected");
    require(result.baseline.tail == 14 && result.candidate.tail == 13, "wrong tail counterexample");
    require(result.baseline.pickup_sum == 4 && result.candidate.pickup_sum == 7,
            "tail objective incorrectly forced pickup improvement");
    require(result.assignment == std::vector<int>({1, 0}), "wrong bottleneck assignment");
}

void test_rectangular_and_fixed_tail()
{
    const tb::Matrix matrix{{0, 3}, {4, 4}, {8, 0}};
    const std::vector<tb::Cost> service{0, 10};
    const auto result = tb::solve(matrix, service, 0, never_cancel);
    require(result.status == tb::Status::optimal, "rectangular solution missing");
    require(result.objective.tail == 10 && result.objective.pickup_sum == 0,
            "rectangular solution wrong");
    require(result.assignment == std::vector<int>({0, -1, 1}), "spare agent not idle");
    const tb::Matrix square{{0, 3}, {4, 4}};
    const auto fixed = tb::solve(square, service, 20, never_cancel);
    require(fixed.objective.tail == 20 && fixed.objective.pickup_sum == 4,
            "fixed loaded tail did not allow minimum pickup tie-break");
    const auto no_improvement = tb::refine(square, service, 20, {0, 1}, never_cancel);
    require(!no_improvement.accepted && no_improvement.assignment == std::vector<int>({0, 1}),
            "equal objective changed baseline");
    const auto pickup_improved = tb::refine(square, service, 20, {1, 0}, never_cancel);
    require(pickup_improved.accepted && pickup_improved.candidate.pickup_sum == 4,
            "secondary pickup improvement rejected");
}

void test_unreachable_and_invalid()
{
    require(tb::solve({{1, -1}, {2, -1}}, {0, 2}, 0, never_cancel).status == tb::Status::infeasible,
            "unreachable task accepted");
    require(tb::solve({{1, 2}}, {0, 0}, 0, never_cancel).status == tb::Status::infeasible,
            "more tasks than agents accepted");
    const auto sparse = tb::solve({{-1, 3}, {4, -1}}, {0, 2}, 0, never_cancel);
    require(sparse.status == tb::Status::optimal && sparse.objective.tail == 5,
            "reachable sparse matching rejected");
    require(tb::solve({{1}, {1, 2}}, {0}, 0, never_cancel).status == tb::Status::invalid_input,
            "ragged input accepted");
    require(tb::solve({{1}}, {-1}, 0, never_cancel).status == tb::Status::invalid_input,
            "negative service length accepted");
    require(tb::solve({{1}}, {0}, -1, never_cancel).status == tb::Status::invalid_input,
            "negative fixed tail accepted");
    require(tb::solve({{tb::kInfinity}}, {0}, 0, never_cancel).status == tb::Status::invalid_input,
            "unsafe arithmetic input accepted");
    const auto empty = tb::solve({{}, {}}, {}, 7, never_cancel);
    require(empty.status == tb::Status::optimal && empty.objective.tail == 7 &&
            empty.assignment == std::vector<int>({-1, -1}), "empty tasks mishandled");
    const auto no_agents = tb::solve({}, {}, 0, never_cancel);
    require(no_agents.status == tb::Status::optimal && no_agents.objective.complete, "empty input mishandled");
    const auto duplicate = tb::refine({{1, 2}, {2, 1}}, {0, 0}, 0, {0, 0}, never_cancel);
    require(duplicate.status == tb::Status::invalid_input && !duplicate.accepted &&
            duplicate.assignment == std::vector<int>({0, 0}), "incomplete baseline overwritten");
}

void test_deadline_fallback()
{
    const tb::Matrix matrix{{0, 3}, {4, 4}, {7, 7}};
    const std::vector<tb::Cost> service{0, 10};
    const std::vector<int> baseline{0, 1, -1};
    int checks = 0;
    const auto full = tb::refine(matrix, service, 0, baseline, [&] { ++checks; return false; });
    require(full.accepted, "deadline fixture must improve");
    require(checks > 10, "deadline fixture did not enter matching loops");
    for (int limit = 1; limit <= checks; ++limit) {
        int elapsed = 0;
        const auto result = tb::refine(matrix, service, 0, baseline, [&] { return ++elapsed >= limit; });
        require(result.status == tb::Status::cancelled && !result.accepted && result.assignment == baseline,
                "partial result escaped cancellation at check " + std::to_string(limit));
    }
    int elapsed = 0;
    const auto complete = tb::refine(matrix, service, 0, baseline, [&] { return ++elapsed > checks; });
    require(complete.status == tb::Status::optimal && complete.accepted,
            "completed result rejected before deadline");
}

void test_determinism_and_enumeration()
{
    std::mt19937 random(542617);
    for (int trial = 0; trial < 1600; ++trial) {
        const int agents = 1 + static_cast<int>(random() % 5);
        const int tasks = static_cast<int>(random() % (agents + 1));
        tb::Matrix matrix(agents, std::vector<tb::Cost>(tasks));
        std::vector<tb::Cost> service(tasks);
        for (auto& length : service) length = random() % 12;
        for (auto& row : matrix)
            for (auto& distance : row) distance = random() % 5 == 0 ? -1 : static_cast<int>(random() % 10);
        const tb::Cost fixed_tail = random() % 22;
        const auto expected = brute_force(matrix, service, fixed_tail);
        const auto result = tb::solve(matrix, service, fixed_tail, never_cancel);
        require((result.status == tb::Status::optimal) == expected.feasible,
                "enumerated feasibility mismatch " + std::to_string(trial));
        if (!expected.feasible) continue;
        require(result.objective.tail == expected.tail && result.objective.pickup_sum == expected.pickup,
                "enumerated optimal objective mismatch " + std::to_string(trial));
        const auto again = tb::solve(matrix, service, fixed_tail, never_cancel);
        require(again.assignment == result.assignment, "matching not deterministic");
    }
    const tb::Matrix tied{{1, 1}, {1, 1}};
    const std::vector<int> baseline{1, 0};
    const auto keep = tb::refine(tied, {0, 0}, 0, baseline, never_cancel);
    require(!keep.accepted && keep.assignment == baseline, "tie caused unnecessary reassignment");
}

int main()
{
    try {
        test_column_constant_counterexample();
        test_rectangular_and_fixed_tail();
        test_unreachable_and_invalid();
        test_deadline_fallback();
        test_determinism_and_enumeration();
        std::cout << "PASS: tail bottleneck assignment; 1600 enumerated cases; every cancellation point; "
                     "rectangular, unreachable, fixed-tail, column-constant and tie fixtures.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
