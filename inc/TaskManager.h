#include "SharedEnv.h"
#include "Tasks.h"
#include "States.h"

#include "nlohmann/json.hpp"
#include <vector>
#include "Logger.h"
#include <string>
#include <unordered_map>

class TaskManager{
public:
    struct TaskMetric {
        int task_id = -1;
        int revealed_at = -1;
        int first_assigned_at = -1;
        int picked_up_at = -1;
        int completed_at = -1;
        int first_agent = -1;
        int current_agent = -1;
        int reassignments = 0;
        int pickup_distance_at_assignment = -1;
        long long empty_distance = 0;
        long long loaded_distance = 0;
        long long active_wait_steps = 0;
        std::vector<int> locations;
    };

    struct AgentMetric {
        long long idle_steps = 0;
        long long empty_steps = 0;
        long long loaded_steps = 0;
        long long active_wait_steps = 0;
        long long empty_distance = 0;
        long long loaded_distance = 0;
        int completed_tasks = 0;
    };

    struct TimelineMetric {
        int timestep = 0;
        int finished_tasks = 0;
        int active_tasks = 0;
        int unassigned_backlog = 0;
        int assigned_agents = 0;
        int idle_agents = 0;
        int empty_agents = 0;
        int loaded_agents = 0;
        long long active_wait_steps = 0;
        double planner_seconds = 0;
    };

    list<std::tuple<int,int,int,int>> events;
    vector<list<pair<int,int>>> actual_schedule;
    vector<list<pair<int,int>>> planner_schedule;
    list<std::tuple<std::string,int,int,int,int>> schedule_errors;

    vector<int> new_freeagents;
    vector<int> new_tasks;

    list<int> check_finished_tasks(vector<State>& states, int timestep);

    int curr_timestep;


    // reveal new task
    void reveal_tasks(int timestep);
    void update_tasks(vector<State>& states, vector<int>& assignment, int timestep);
    void record_agent_step(const vector<State>& before, const vector<State>& after);
    void record_timeline(int timestep, double planner_seconds);
    void save_metrics(const std::string& result_file) const;

    void sync_shared_env(SharedEnvironment* env);

    void set_num_tasks_reveal(float num){num_tasks_reveal = num*num_of_agents;};
    void set_logger(Logger* logger){this->logger = logger;}

    bool validate_task_assignment(vector<int>& assignment); // validate the task assignment
    bool set_task_assignment(vector<int>& assignment, const vector<State>* states = nullptr);

    int get_number_errors() const {return schedule_errors.size();}



    TaskManager(std::vector<list<int>>& tasks, int num_of_agents):
        tasks(tasks), num_of_agents(num_of_agents)
    {
        finished_tasks.resize(num_of_agents);
        current_assignment.resize(num_of_agents);
        for (auto & t: current_assignment)
            t = -1;
        // events.resize(num_of_agents);
        actual_schedule.resize(num_of_agents);
        planner_schedule.resize(num_of_agents);
        agent_metrics.resize(num_of_agents);
    }

    nlohmann::ordered_json to_json(int map_cols) const;



    int num_of_task_finish = 0;

    ~ TaskManager()
    {
        for (Task* task: all_tasks){
            delete task;
        }
    }

private:
    Logger* logger = nullptr;

    std::unordered_map<int, Task*> ongoing_tasks;
    vector<int> current_assignment;
    std::unordered_map<int, TaskMetric> task_metrics;
    std::vector<AgentMetric> agent_metrics;
    std::vector<TimelineMetric> timeline_metrics;
    SharedEnvironment* metric_env = nullptr;

    int num_tasks_reveal = 1;
    int num_of_agents;

    std::vector<std::list<Task* > > finished_tasks; // location + finish time

    list<Task*> all_tasks;


    std::vector<list<int>>& tasks;
    int task_id = 0;

};
