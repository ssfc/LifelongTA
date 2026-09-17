#pragma once
#include "Tasks.h"
#include "SharedEnv.h"
#include "scheduler.h"
#include "portable_assignment.h"


class TaskScheduler
{
    public:
        SharedEnvironment* env;

        TaskScheduler(SharedEnvironment* env): env(env){};
        TaskScheduler(){env = new SharedEnvironment();};
        virtual ~TaskScheduler(){delete env;};
        virtual void initialize(int preprocess_time_limit);
        virtual void plan(int time_limit, std::vector<int> & proposed_schedule);

        void set_flow(std::vector<DefaultPlanner::Double4> flow);

        void set_use_traffic(bool use_traffic);
        void set_new_only(bool new_only);
        void set_solver(int solver);
        void set_max_matching_edges(int max_matching_edges);
        void set_heap_config(const DefaultPlanner::PortableGreedyHeapConfig& config);
        void set_task_matcher_config(const DefaultPlanner::PortableTaskMatcherConfig& config);
        void set_capped_hungarian_config(const DefaultPlanner::PortableCappedHungarianConfig& config);

        std::vector<DefaultPlanner::Double4> background_flow;

        bool use_traffic = false;
        bool new_only = false;
        int solver = 1; //1 matching, 2 flow
        int max_matching_edges = INT_MAX;
        DefaultPlanner::PortableGreedyHeapConfig heap_config;
        DefaultPlanner::PortableTaskMatcherConfig task_matcher_config;
        DefaultPlanner::PortableCappedHungarianConfig capped_hungarian_config;

};
