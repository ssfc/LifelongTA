#include "TaskManager.h"
#include "heuristics.h"
#include <cassert>
#include <fstream>
#include <sstream>

// Isolate TaskManager telemetry from planner timing and logger side effects.
Logger::Logger(std::string, int) {}
void Logger::log_info(std::string) {}
void Logger::log_info(std::string, int) {}
void Logger::log_warning(std::string) {}
void Logger::log_warning(std::string, int) {}
void Logger::log_fatal(std::string) {}
void Logger::log_fatal(std::string, int) {}
void Logger::flush() {}
namespace DefaultPlanner {
Neighbors global_neighbors;
int get_h(SharedEnvironment*, int source, int target) { return std::abs(source - target); }
int get_d(int diff, const SharedEnvironment*) { return diff > 0 ? 0 : 2; }
}

std::unordered_map<std::string, std::string> first_task(const std::string& base)
{
    std::ifstream input(base + ".task_metrics.csv");
    std::string header, row, key, value;
    std::getline(input, header);
    std::getline(input, row);
    std::istringstream headers(header), values(row);
    std::unordered_map<std::string, std::string> result;
    while (std::getline(headers, key, ',') && std::getline(values, value, ',')) result[key] = value;
    return result;
}

int main()
{
    Logger logger("", 5);
    SharedEnvironment env;
    env.rows = 1; env.cols = 12;
    env.assignment_flow.resize(12);
    DefaultPlanner::global_neighbors.resize(12);
    for (int i = 0; i < 12; ++i) {
        if (i) DefaultPlanner::global_neighbors[i].push_back(i - 1);
        if (i < 11) DefaultPlanner::global_neighbors[i].push_back(i + 1);
    }
    {
        std::vector<std::list<int>> tasks{{2, 4}};
        TaskManager manager(tasks, 1);
        manager.set_logger(&logger); manager.set_num_tasks_reveal(1);
        manager.reveal_tasks(0); manager.sync_shared_env(&env);
        assert(env.task_source_exhausted && manager.get_total_task_count() == 1);
        std::vector<int> assignment{0};
        std::vector<State> before{State(0, 0)}, after{State(1, 1)};
        manager.update_tasks(after, assignment, 1, &before);
        before = after; after = {State(2, 2)};
        manager.update_tasks(after, assignment, 2, &before);
        before = after; after = {State(3, 3)};
        manager.update_tasks(after, assignment, 3, &before);
        before = after; after = {State(3, 4)};
        manager.record_agent_step(before, after); // timeout: no pickup/completion update
        assert(manager.num_of_task_finish == 0);
        before = after; after = {State(4, 5)};
        manager.update_tasks(after, assignment, 5, &before);
        assert(manager.finish_all_tasks && manager.num_of_task_finish == 1);
        const std::string base = "build-tail-bottleneck/telemetry-smoke-linear";
        manager.save_metrics(base + ".json");
        const auto metric = first_task(base);
        assert(metric.at("first_assigned_at") == "0");
        assert(metric.at("pickup_distance_at_assignment") == "2");
        assert(metric.at("empty_distance") == "2" && metric.at("loaded_distance") == "2");
        assert(metric.at("loaded_wait_steps") == "1" && metric.at("completed_at") == "5");
        std::ifstream summary_file(base + ".metrics_summary.json");
        nlohmann::ordered_json summary; summary_file >> summary;
        assert(summary["format"] == "lifelongta-metrics-v2");
        assert(summary["agentStateSteps"]["idle"] == 0);
        assert(summary["agentStateSteps"]["empty"] == 2);
        assert(summary["agentStateSteps"]["loaded"] == 3);
    }
    {
        std::vector<std::list<int>> tasks{{9, 10}};
        TaskManager manager(tasks, 2);
        manager.set_logger(&logger); manager.set_num_tasks_reveal(1);
        manager.reveal_tasks(0); manager.sync_shared_env(&env);
        std::vector<State> before{State(0, 0), State(2, 0)};
        const std::vector<std::vector<int>> schedules{{0, -1}, {-1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {0, 0}};
        for (int step = 0; step < static_cast<int>(schedules.size()); ++step) {
            auto assignment = schedules[step];
            std::vector<State> after{State(0, step + 1), State(2, step + 1)};
            manager.update_tasks(after, assignment, step + 1, &before);
            before = after;
        }
        assert(manager.get_number_errors() == 1); // invalid duplicate reported only once
        manager.sync_shared_env(&env);
        assert(env.curr_task_schedule == std::vector<int>({-1, 0}));
        const std::string base = "build-tail-bottleneck/telemetry-smoke-reassign";
        manager.save_metrics(base + ".json");
        const auto metric = first_task(base);
        assert(metric.at("first_assigned_at") == "0" && metric.at("first_agent") == "0");
        assert(metric.at("reassignments") == "1");
        assert(metric.at("active_wait_steps") == "4");
    }
    {
        std::vector<std::list<int>> tasks{{2, 3}};
        TaskManager manager(tasks, 1);
        manager.set_logger(&logger); manager.set_num_tasks_reveal(1);
        manager.reveal_tasks(0); manager.sync_shared_env(&env);
        std::vector<int> assignment{0};
        std::vector<State> before{State(2, 0)}, after{State(3, 1)};
        manager.update_tasks(after, assignment, 1, &before);
        manager.sync_shared_env(&env);
        assert(env.task_pool.at(0).idx_next_loc == 0 && manager.num_of_task_finish == 0);
    }
    std::cout << "PASS: first-step attribution, pre-move sampling, timeout conservation, reassignment gaps, invalid schedule fallback, and original pickup order\n";
}
