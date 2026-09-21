#ifndef PLANNER
#define PLANNER

#include "Types.h"
#include "TrajLNS.h"
#include <random>


namespace DefaultPlanner{

struct PlannerAgentSnapshot {
    int goal = -1;
    double priority = 0.0;
    bool in_deadend = false;
    std::vector<int> guide_prefix;
};

using PlannerSnapshot = std::vector<PlannerAgentSnapshot>;

    
    void initialize(int preprocess_time_limit, SharedEnvironment* env);

    void plan(int time_limit,vector<Action> & actions,  SharedEnvironment* env, unordered_map<int,list<int>> agent_guide_path);

    // std::vector<Int4> get_flow();

    std::vector<Double4> get_opened_flow(SharedEnvironment* env);
    PlannerSnapshot get_snapshot(SharedEnvironment* env, int prefix_length = 8);
    void plan_pibt(int time_limit,vector<Action> & actions, SharedEnvironment* env);


}
#endif
