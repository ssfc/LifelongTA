#include "TaskManager.h"
#include "Tasks.h"
#include "heuristics.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>

using json = nlohmann::ordered_json;

/**
 * This function validates the proposed schedule (assignment) from participants
 * 
 * @param assignment a vector of task_ids, one for each agent. The length of the vector should be equal to the number of agents.
 *
 */
bool TaskManager::validate_task_assignment(vector<int>& assignment)
{
    if (assignment.size() != num_of_agents)
    {
        schedule_errors.push_back(make_tuple("Invalid schedule size",-1,-1,-1,curr_timestep+1));
        logger->log_warning("Scheduler Error: assignment size does not match number of agents",curr_timestep+1);
        return false;
    }

    unordered_map<int,int> idx_set;

    //here we only check the first assignment to each agent
    for (int i_agent = 0; i_agent < assignment.size(); i_agent ++)
    {
        // task should be a ongoing task
        if (assignment[i_agent] != -1 && ongoing_tasks.find(assignment[i_agent]) == ongoing_tasks.end())
        {
            schedule_errors.push_back(make_tuple("task already finished",assignment[i_agent],i_agent,-1,curr_timestep+1));
            logger->log_warning("Scheduler Error: schedule agent " + std::to_string(i_agent) + " to task " + std::to_string(assignment[i_agent]) + " wrong because the task is already finished",curr_timestep+1);
            logger->flush();
            return false;
        }

        // one task should not appear in the assignment twice
        if (assignment[i_agent] != -1 && idx_set.find(assignment[i_agent]) != idx_set.end())
        {
            schedule_errors.push_back(make_tuple("task is already assigned by the second agent at the same time",assignment[i_agent],i_agent,idx_set[assignment[i_agent]],curr_timestep+1));
            logger->log_warning("Scheduler Error: schedule agent " + std::to_string(i_agent) + " to task " + std::to_string(assignment[i_agent]) + " wrong because the task is already assigned to agent " + std::to_string(idx_set[assignment[i_agent]]),curr_timestep+1);
            return false;
        }

        // if agent is already executing some task, it should be assigned the same task.
        if (current_assignment[i_agent] != -1)
        {
            if (ongoing_tasks[current_assignment[i_agent]]->idx_next_loc > 0 && (current_assignment[i_agent] == -1  || assignment[i_agent] != current_assignment[i_agent]))
            {
                schedule_errors.push_back(make_tuple("task is already opened by the second agent",assignment[i_agent],i_agent,ongoing_tasks[current_assignment[i_agent]]->agent_assigned,curr_timestep+1));
                logger->log_warning("Scheduler Error: schedule agent " + std::to_string(i_agent) + " to task " + std::to_string(assignment[i_agent]) + " wrong because the task is already opened by agent " + std::to_string(ongoing_tasks[current_assignment[i_agent]]->agent_assigned),curr_timestep+1);
                return false;
            }
        }
        if (assignment[i_agent] != -1)
        {
            idx_set[assignment[i_agent]] = i_agent;
        }
    }

    return true;
}


/**
 * This function updates the current task assignments of agents.
 * It first checks if the proposed assignment is valid, 
 * then updates the current assignment and updates the corresponding agent_assigned of each affected task.
 * 
 * @param assignment a vector of task_ids, one for each agent. The length of the vector should be equal to the number of agents.
 *
 */
bool TaskManager::set_task_assignment(vector<int>& assignment, const vector<State>* states)
{
    // for (int a = 0; a < assignment.size(); a++)
    // {
    //     if (planner_schedule[a].empty() || assignment[a] != planner_schedule[a].back().second)
    //     {
    //         planner_schedule[a].push_back(make_pair(curr_timestep,assignment[a]));
    //     }
    // }
    if (! validate_task_assignment(assignment))
    {
        return false;
    }

    std::unordered_map<int, int> next_agent;
    for (int agent = 0; agent < static_cast<int>(assignment.size()); ++agent)
        if (assignment[agent] >= 0) next_agent[assignment[agent]] = agent;
    for (const auto& next : next_agent)
    {
        auto metric_it = task_metrics.find(next.first);
        if (metric_it == task_metrics.end()) continue;
        TaskMetric& metric = metric_it->second;
        if (metric.current_agent == next.second) continue;
        if (metric.first_assigned_at < 0)
        {
            metric.first_assigned_at = curr_timestep;
            metric.first_agent = next.second;
            if (metric_env != nullptr && states != nullptr && next.second < static_cast<int>(states->size()) &&
                !metric.locations.empty())
                metric.pickup_distance_at_assignment = DefaultPlanner::get_h(metric_env,
                    states->at(next.second).location, metric.locations.front());
        }
        else if (metric.current_agent >= 0)
        {
            ++metric.reassignments;
        }
        metric.current_agent = next.second;
    }
    for (auto& entry : task_metrics)
        if (entry.second.current_agent >= 0 && next_agent.find(entry.first) == next_agent.end())
            entry.second.current_agent = -1;

    //reset all the agent_assigned to -1, so that any droped task->agent_assignment will be -1
    for (int a = 0; a < assignment.size(); a++)
    {
        if (current_assignment[a] >= 0){
            ongoing_tasks[current_assignment[a]]->agent_assigned = -1;
        }
    }

    // then set the updated agent_assigned according to new assignments.
    for (int a = 0; a < assignment.size(); a++)
    {
        int t_id = assignment[a];
        current_assignment[a] = t_id;
        if (assignment[a] < 0)
        {
            continue;
        }
        ongoing_tasks[t_id]->agent_assigned = a;
    }
    
    // cout<<"assignments:"<<endl;
    // for (int a = 0; a < current_assignment.size(); a++)
    // {
    //     if (actual_schedule[a].empty() || current_assignment[a] != actual_schedule[a].back().second)
    //     {
    //         //actual_schedule[a].push_back(make_pair(curr_timestep,current_assignment[a]));
    //         cout<<"(Agent "<<a<<",Task "<<current_assignment[a]<<")";
    //     }
    // }
    // cout<<endl;

    return true;
}

/**
 * This function checks if any task is finished at the current timestep.
 * If a task is finished, it updates the task's completion time and the agent's current assignment.
 * 
 * @param states a vector of states of all agents, including the current location of each agent on the map.
 * @param timestep the current timestep.
 */
list<int> TaskManager::check_finished_tasks(vector<State>& states, int timestep)
{ 
    list<int> finished_tasks_this_timestep; // <agent_id, task_id, timestep>
    new_freeagents.clear(); //prepare to push all new free agents to the shared environment
    for (int k = 0; k < num_of_agents; k++)
    {
        if (current_assignment[k] != -1 && states[k].location == ongoing_tasks[current_assignment[k]]->get_next_loc())
        {
            Task * task = ongoing_tasks[current_assignment[k]];
            task->idx_next_loc += 1;

            if (task->is_finished())
            {
                current_assignment[k] = -1;
                ongoing_tasks.erase(task->task_id);
                task->t_completed = timestep;

                finished_tasks_this_timestep.push_back(task->task_id);
                finished_tasks[task->agent_assigned].emplace_back(task);
                num_of_task_finish++;
                auto metric_it = task_metrics.find(task->task_id);
                if (metric_it != task_metrics.end())
                {
                    metric_it->second.completed_at = timestep;
                    metric_it->second.current_agent = -1;
                }
                if (task->agent_assigned >= 0 && task->agent_assigned < static_cast<int>(agent_metrics.size()))
                    ++agent_metrics[task->agent_assigned].completed_tasks;
                new_freeagents.push_back(k); // record the new free agent
                logger->log_info("Agent " + std::to_string(task->agent_assigned) + " finishes task " + std::to_string(task->task_id), timestep);
                logger->flush();
            }
            else if (task->idx_next_loc>0)
            {
                auto metric_it = task_metrics.find(task->task_id);
                if (metric_it != task_metrics.end() && metric_it->second.picked_up_at < 0)
                    metric_it->second.picked_up_at = timestep;
                logger->log_info("Agent " + std::to_string(task->agent_assigned) + " opens task " + std::to_string(task->task_id), timestep);
            }
            //events.push_back(make_tuple(timestep,k,task->task_id,task->idx_next_loc));
        }
    }
    return finished_tasks_this_timestep;
}

/**
 * This function synchronises the shared environment with the current task manager.
 * It copies the current task pool, current task schedule, new free agents, and new tasks to the shared environment.
 * 
 * @param env a pointer to the shared environment.
 */
void TaskManager::sync_shared_env(SharedEnvironment* env) 
{
    metric_env = env;
    env->task_pool.clear();
    for (auto& task: ongoing_tasks)
    {
        env->task_pool[task.first] = *task.second;
    }
    env->curr_task_schedule = current_assignment;
    env->new_freeagents = new_freeagents;
    env->new_tasks = new_tasks; 
}

/**
 * This function reveals new tasks at the current timestep.
 * It reveals a fixed number of tasks at each timestep, 
 * and adds them to the ongoing tasks, new_tasks, and all_tasks.
 * 
 * @param timestep the current timestep.
 */
void TaskManager::reveal_tasks(int timestep)
{
    new_tasks.clear(); //prepare to push all new revealed tasks to the shared environment
    while (ongoing_tasks.size() < num_tasks_reveal)
    {
        int i = task_id%tasks.size();
        list<int> locs = tasks[i];
        Task* task = new Task(task_id,locs,timestep);
        ongoing_tasks[task->task_id] = task;
        task_metrics.emplace(task->task_id, TaskMetric{task->task_id, timestep, -1, -1, -1,
            -1, -1, 0, -1, 0, 0, 0, task->locations});
        //all_tasks.push_back(task);
        new_tasks.push_back(task->task_id);         // record the new tasks
        logger->log_info("Task " + std::to_string(task_id) + " is revealed");
        task_id++;
    }
}

/**
 * This function is reponsible for the task management process:
 * 1. It updates the current assignments of agents with proposed schedule from participants.
 * 2. It checks if any task is finished at the current timestep.
 * 3. It reveals new tasks at the current timestep.
 * 
 * @param states a vector of states of all agents, including the current location of each agent on the map.
 * @param assignment a vector of task_ids, one for each agent. The length of the vector should be equal to the number of agents.
 * @param timestep the current timestep.
 */
void TaskManager::update_tasks(vector<State>& states, vector<int>& assignment, int timestep)
{
    curr_timestep = timestep;
    set_task_assignment(assignment, &states);
    check_finished_tasks(states,timestep);
    reveal_tasks(timestep);
}

void TaskManager::record_agent_step(const vector<State>& before, const vector<State>& after)
{
    const int count = std::min({num_of_agents, static_cast<int>(before.size()), static_cast<int>(after.size())});
    for (int agent = 0; agent < count; ++agent)
    {
        const int task_id = current_assignment[agent];
        const bool moved = before[agent].location != after[agent].location;
        if (task_id < 0) { ++agent_metrics[agent].idle_steps; continue; }
        const auto task_it = ongoing_tasks.find(task_id);
        if (task_it == ongoing_tasks.end()) continue;
        const bool loaded = task_it->second->idx_next_loc > 0;
        auto metric_it = task_metrics.find(task_id);
        TaskMetric* task_metric = metric_it == task_metrics.end() ? nullptr : &metric_it->second;

        if (loaded) ++agent_metrics[agent].loaded_steps;
        else ++agent_metrics[agent].empty_steps;
        if (!moved)
        {
            ++agent_metrics[agent].active_wait_steps;
            if (task_metric != nullptr) ++task_metric->active_wait_steps;
        }
        else if (loaded)
        {
            ++agent_metrics[agent].loaded_distance;
            if (task_metric != nullptr) ++task_metric->loaded_distance;
        }
        else
        {
            ++agent_metrics[agent].empty_distance;
            if (task_metric != nullptr) ++task_metric->empty_distance;
        }
    }
}

void TaskManager::record_timeline(int timestep, double planner_seconds)
{
    TimelineMetric metric;
    metric.timestep = timestep;
    metric.finished_tasks = num_of_task_finish;
    metric.active_tasks = static_cast<int>(ongoing_tasks.size());
    metric.planner_seconds = planner_seconds;
    for (const auto& task : ongoing_tasks)
        if (task.second->agent_assigned < 0) ++metric.unassigned_backlog;
    for (int agent = 0; agent < num_of_agents; ++agent)
    {
        const int task_id = current_assignment[agent];
        if (task_id < 0) { ++metric.idle_agents; continue; }
        ++metric.assigned_agents;
        const auto task_it = ongoing_tasks.find(task_id);
        if (task_it != ongoing_tasks.end() && task_it->second->idx_next_loc > 0) ++metric.loaded_agents;
        else ++metric.empty_agents;
    }
    for (const auto& agent : agent_metrics) metric.active_wait_steps += agent.active_wait_steps;
    timeline_metrics.push_back(metric);
}

void TaskManager::save_metrics(const std::string& result_file) const
{
    const std::string base = result_file.size() >= 5 && result_file.substr(result_file.size() - 5) == ".json"
        ? result_file.substr(0, result_file.size() - 5) : result_file;
    std::vector<const TaskMetric*> ordered_tasks;
    ordered_tasks.reserve(task_metrics.size());
    for (const auto& entry : task_metrics) ordered_tasks.push_back(&entry.second);
    std::sort(ordered_tasks.begin(), ordered_tasks.end(),
              [](const TaskMetric* lhs, const TaskMetric* rhs) { return lhs->task_id < rhs->task_id; });

    std::ofstream tasks_out(base + ".task_metrics.csv", std::ios::trunc);
    tasks_out << "task_id,revealed_at,first_assigned_at,picked_up_at,completed_at,first_agent,reassignments,"
                 "pickup_distance_at_assignment,service_shortest_distance,empty_distance,loaded_distance,"
                 "active_wait_steps,assignment_wait,pickup_wait,completion_time,loaded_detour_ratio\n";
    std::vector<double> assignment_waits, pickup_waits, completion_times, pickup_distances, detour_ratios;
    long long total_empty_distance = 0, total_loaded_distance = 0, total_service_distance = 0;
    for (const TaskMetric* task_ptr : ordered_tasks)
    {
        const TaskMetric& task = *task_ptr;
        int service_distance = -1;
        if (metric_env != nullptr && task.locations.size() > 1)
        {
            service_distance = 0;
            for (size_t i = 1; i < task.locations.size(); ++i)
                service_distance += DefaultPlanner::get_h(metric_env, task.locations[i - 1], task.locations[i]);
        }
        const int assignment_wait = task.first_assigned_at < 0 ? -1 : task.first_assigned_at - task.revealed_at;
        const int pickup_wait = task.picked_up_at < 0 ? -1 : task.picked_up_at - task.revealed_at;
        const int completion_time = task.completed_at < 0 ? -1 : task.completed_at - task.revealed_at;
        tasks_out << task.task_id << ',' << task.revealed_at << ',' << task.first_assigned_at << ','
                  << task.picked_up_at << ',' << task.completed_at << ',' << task.first_agent << ','
                  << task.reassignments << ',' << task.pickup_distance_at_assignment << ',' << service_distance << ','
                  << task.empty_distance << ',' << task.loaded_distance << ',' << task.active_wait_steps << ','
                  << assignment_wait << ',' << pickup_wait << ',' << completion_time << ',';
        if (service_distance > 0) tasks_out << std::fixed << std::setprecision(6)
                                             << static_cast<double>(task.loaded_distance) / service_distance;
        tasks_out << '\n';
        total_empty_distance += task.empty_distance;
        total_loaded_distance += task.loaded_distance;
        if (task.completed_at >= 0)
        {
            if (assignment_wait >= 0) assignment_waits.push_back(assignment_wait);
            if (pickup_wait >= 0) pickup_waits.push_back(pickup_wait);
            if (completion_time >= 0) completion_times.push_back(completion_time);
            if (task.pickup_distance_at_assignment >= 0) pickup_distances.push_back(task.pickup_distance_at_assignment);
            if (service_distance > 0)
            {
                total_service_distance += service_distance;
                detour_ratios.push_back(static_cast<double>(task.loaded_distance) / service_distance);
            }
        }
    }

    std::ofstream agents_out(base + ".agent_metrics.csv", std::ios::trunc);
    agents_out << "agent_id,completed_tasks,idle_steps,empty_steps,loaded_steps,active_wait_steps,empty_distance,loaded_distance\n";
    for (int agent = 0; agent < static_cast<int>(agent_metrics.size()); ++agent)
    {
        const AgentMetric& metric = agent_metrics[agent];
        agents_out << agent << ',' << metric.completed_tasks << ',' << metric.idle_steps << ',' << metric.empty_steps
                   << ',' << metric.loaded_steps << ',' << metric.active_wait_steps << ',' << metric.empty_distance
                   << ',' << metric.loaded_distance << '\n';
    }

    std::ofstream timeline_out(base + ".timeline_metrics.csv", std::ios::trunc);
    timeline_out << "timestep,finished_tasks,active_tasks,unassigned_backlog,assigned_agents,idle_agents,empty_agents,loaded_agents,active_wait_steps_cumulative,planner_seconds\n";
    for (const TimelineMetric& metric : timeline_metrics)
        timeline_out << metric.timestep << ',' << metric.finished_tasks << ',' << metric.active_tasks << ','
                     << metric.unassigned_backlog << ',' << metric.assigned_agents << ',' << metric.idle_agents << ','
                     << metric.empty_agents << ',' << metric.loaded_agents << ',' << metric.active_wait_steps << ','
                     << std::setprecision(9) << metric.planner_seconds << '\n';

    const auto summarize = [](std::vector<double> values) {
        json summary;
        if (values.empty()) return summary;
        std::sort(values.begin(), values.end());
        const auto percentile = [&values](double p) {
            const size_t index = static_cast<size_t>(std::ceil(p * values.size())) - 1;
            return values[std::min(index, values.size() - 1)];
        };
        double total = 0;
        for (const double value : values) total += value;
        summary["count"] = values.size();
        summary["mean"] = total / values.size();
        summary["p50"] = percentile(0.50);
        summary["p95"] = percentile(0.95);
        summary["p99"] = percentile(0.99);
        summary["max"] = values.back();
        return summary;
    };

    long long idle_steps = 0, empty_steps = 0, loaded_steps = 0, active_wait_steps = 0;
    for (const AgentMetric& metric : agent_metrics)
    {
        idle_steps += metric.idle_steps;
        empty_steps += metric.empty_steps;
        loaded_steps += metric.loaded_steps;
        active_wait_steps += metric.active_wait_steps;
    }
    std::vector<double> planner_seconds;
    for (const TimelineMetric& metric : timeline_metrics) planner_seconds.push_back(metric.planner_seconds);
    int unassigned_backlog = 0;
    for (const auto& task : ongoing_tasks)
        if (task.second->agent_assigned < 0) ++unassigned_backlog;

    json summary;
    summary["format"] = "lifelongta-metrics-v1";
    summary["tasksRevealed"] = ordered_tasks.size();
    summary["tasksCompleted"] = num_of_task_finish;
    summary["activeTasksAtEnd"] = ongoing_tasks.size();
    summary["unassignedBacklogAtEnd"] = unassigned_backlog;
    summary["taskLatency"] = {{"arrivalToAssignment", summarize(assignment_waits)},
                              {"arrivalToPickup", summarize(pickup_waits)},
                              {"arrivalToCompletion", summarize(completion_times)}};
    summary["matchingQuality"] = {{"pickupDistanceAtAssignment", summarize(pickup_distances)}};
    summary["movement"] = {{"emptyDistance", total_empty_distance}, {"loadedDistance", total_loaded_distance},
                           {"completedTaskServiceShortestDistance", total_service_distance},
                           {"loadedDetourRatio", summarize(detour_ratios)},
                           {"productiveMovementRatio", total_empty_distance + total_loaded_distance == 0 ? 0.0 :
                            static_cast<double>(total_service_distance) / (total_empty_distance + total_loaded_distance)}};
    summary["agentStateSteps"] = {{"idle", idle_steps}, {"empty", empty_steps},
                                  {"loaded", loaded_steps}, {"activeWait", active_wait_steps}};
    summary["plannerSeconds"] = summarize(planner_seconds);
    std::ofstream summary_out(base + ".metrics_summary.json", std::ios::trunc);
    summary_out << std::setw(2) << summary;
}

/**
 * This function converts all tasks to a JSON object.
 * 
 * @param map_cols the number of columns in the map.
 */
json TaskManager::to_json(int map_cols) const
{
    
    json tasks = json::array();
    for (auto t: all_tasks)
    {
        json task = json::array();
        task.push_back(t->task_id);
        // TODO rewrite the task output part
        // task.push_back(t->locations.front()/map_cols);
        // task.push_back(t->locations.front()%map_cols);
        task.push_back(t->t_revealed);
        json locs = json::array();
        for (auto loc: t->locations)
        {
            locs.push_back(loc/map_cols);
            locs.push_back(loc%map_cols);
        }
        task.push_back(locs);
        tasks.push_back(task);
    }
    return tasks;
}
