#include "CompetitionSystem.h"
#include "Evaluation.h"
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/tokenizer.hpp>
#include "nlohmann/json.hpp"
#include <signal.h>
#include <climits>
#include <memory>


#ifdef PYTHON
#if PYTHON
#include "pyMAPFPlanner.hpp"
#include <pybind11/embed.h>
#include "pyEntry.hpp"
#include "pyTaskScheduler.hpp"
#endif
#endif

namespace po = boost::program_options;
using json = nlohmann::json;

po::variables_map vm;
std::unique_ptr<BaseSystem> system_ptr;


void sigint_handler(int a)
{
    fprintf(stdout, "stop the simulation...\n");
    system_ptr->saveResults(vm["output"].as<std::string>(),vm["outputScreen"].as<int>());
    _exit(0);
}


int main(int argc, char **argv)
{
#ifdef PYTHON
#if PYTHON
    pybind11::initialize_interpreter();
#endif
#endif
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")
        ("inputFile,i", po::value<std::string>()->required(), "input file name")
        ("output,o", po::value<std::string>()->default_value("./output.json"), "output results from the evaluation into a JSON formated file. If no file specified, the default name is 'output.json'")
        ("outputScreen,c", po::value<int>()->default_value(3), "the level of details in the output file, 1--showing all the output, 2--ignore the events and tasks, 3--ignore the events, tasks, errors, planner times, starts and paths")
        ("simulationTime,s", po::value<int>()->default_value(5000), "run simulation")
        ("fileStoragePath,f", po::value<std::string>()->default_value(""), "the large file storage path")
        ("planTimeLimit,t", po::value<int>()->default_value(1000), "the time limit for planner in milliseconds")
        ("preprocessTimeLimit,p", po::value<int>()->default_value(30000), "the time limit for preprocessing in milliseconds")
        ("logFile,l", po::value<std::string>()->default_value(""), "redirect stdout messages into the specified log file")
        ("logDetailLevel,d", po::value<int>()->default_value(1), "the minimum severity level of log messages to display, 1--showing all the messages, 2--showing warnings and fatal errors, 3--showing fatal errors only")
        ("useTraffic,u", po::value<bool>()->default_value(false), "use of traffic in scheduling")
        ("assignNew,n", po::value<bool>()->default_value(false), "wether new agents only or allow task swapping")
        ("scheduleModel,m", po::value<int>()->default_value(1), "scheduler model, 1- flow, 2- flow with history edge cost, 3- matching + dijkstra, 4- matching + lazily stored h, 5- greedy, 6- greedy heap, 7- contest task matcher, 8- capped Hungarian")
        ("heapDistWeight", po::value<float>()->default_value(5.0f), "agent-to-pickup distance weight for greedy heap")
        ("heapMaxAssign", po::value<float>()->default_value(1.0f), "maximum fraction of agents carrying a task in greedy heap")
        ("heapReassign", po::value<bool>()->default_value(true), "enable stable unopened-task reassignment for greedy heap")
        ("heapKeepBias", po::value<float>()->default_value(6.0f), "old-pair bias during greedy-heap reassignment")
        ("heapProtectDist", po::value<int>()->default_value(10), "protect assignments this close to pickup")
        ("heapRebuildPct", po::value<int>()->default_value(45), "greedy-heap candidate rebuild budget percentage")
        ("heapLnsPct", po::value<int>()->default_value(10), "greedy-heap swap-refinement budget percentage")
        ("heapSortK", po::value<int>()->default_value(500), "sorted candidates retained per agent")
        ("heapFlowAwareSpatial", po::value<bool>()->default_value(false), "use spatial candidates and planner-flow congestion penalties in greedy heap")
        ("heapSpatialCell", po::value<int>()->default_value(16), "grid cell side length for flow-aware greedy heap candidates")
        ("heapSpatialCandidates", po::value<int>()->default_value(1000), "maximum nearby tasks scored per agent in flow-aware greedy heap")
        ("heapTrafficWeight", po::value<float>()->default_value(1.0f), "planner-flow congestion penalty weight in flow-aware greedy heap")
        ("heapReassignMinGain", po::value<float>()->default_value(0.0f), "minimum score reduction required for a greedy-heap reassignment swap")
        ("heapFutureFlowCell", po::value<int>()->default_value(32), "zone side length for dynamic future-flow matching")
        ("heapFutureFlowWeight", po::value<float>()->default_value(0.0f), "weight of newly assigned zone future-flow penalties in greedy heap")
        ("heapFutureFlowHardCap", po::value<float>()->default_value(0.0f), "block opposing movement into zones above this dynamic flow level; 0 disables blocking")
        ("heapFutureFlowRegret", po::value<bool>()->default_value(true), "prioritize agents with high candidate regret in dynamic future-flow matching")
        ("heapOpenedFlowZoneRows", po::value<int>()->default_value(0), "rows in the opened-task coarse-flow grid; 0 disables it")
        ("heapOpenedFlowZoneCols", po::value<int>()->default_value(0), "columns in the opened-task coarse-flow grid; 0 disables it")
        ("heapOpenedFlowPenaltyWeight", po::value<float>()->default_value(0.0f), "opened-task coarse-flow penalty weight in greedy heap")
        ("matcherDistWeight", po::value<float>()->default_value(5.0f), "agent-to-pickup distance weight for contest task matcher")
        ("matcherTopK", po::value<int>()->default_value(50), "task matcher top-K candidates in large instances")
        ("matcherMaxMatrix", po::value<int>()->default_value(2000000), "maximum task matcher cost-matrix entries")
        ("matcherMaxAssign", po::value<float>()->default_value(1.0f), "maximum fraction of task-matcher candidates assigned per tick")
        ("matcherAdaptiveAssign", po::value<bool>()->default_value(false), "increase task-matcher assignment capacity when unpicked-task pressure is high")
        ("matcherWaitPriorityWeight", po::value<float>()->default_value(0.0f), "per-timestep TaskMatcher cost bonus for tasks waiting past matcherWaitPriorityThreshold")
        ("matcherWaitPriorityThreshold", po::value<int>()->default_value(0), "waiting time before TaskMatcher wait priority starts")
        ("matcherUseTraffic", po::value<bool>()->default_value(false), "use Flow-Traffic edge costs in task matcher")
        ("matcherTrafficPressure", po::value<float>()->default_value(0.0f), "minimum task-to-candidate pressure before TaskMatcher uses traffic-aware costs; 0 disables the gate")
        ("matcherUseWaitHeat", po::value<bool>()->default_value(false), "add recent MAPF waiting heat to traffic-aware TaskMatcher costs under task pressure")
        ("matcherWaitHeatWeight", po::value<float>()->default_value(0.0f), "weight of recent MAPF waiting heat in traffic-aware TaskMatcher costs")
        ("matcherWaitHeatPressure", po::value<float>()->default_value(1.0f), "minimum task-to-candidate pressure for waiting-heat costs")
        ("matcherGuideRegretWeight", po::value<float>()->default_value(0.0f), "per-guide-step keep bias for reassigning a TaskMatcher agent")
        ("matcherGuideRegretCap", po::value<int>()->default_value(20), "maximum guide-path steps counted by TaskMatcher reassignment regret")
        ("matcherTailRescueWeight", po::value<float>()->default_value(0.0f), "per-timestep capped priority for overdue TaskMatcher tasks")
        ("matcherTailRescueThreshold", po::value<int>()->default_value(60), "task age before capped TaskMatcher tail rescue begins")
        ("matcherTailRescueCap", po::value<int>()->default_value(20), "maximum overdue timesteps counted by TaskMatcher tail rescue")
        ("matcherTrafficTopK", po::value<int>()->default_value(50), "static nearest pickups rescored with traffic-aware Dijkstra per agent")
        ("matcherTrafficCongestionWeight", po::value<float>()->default_value(1.0f), "multiplier for TaskMatcher traffic congestion penalties")
        ("matcherReassign", po::value<bool>()->default_value(true), "enable unopened-task reassignment for task matcher")
        ("finiteTaskStream", po::value<bool>()->default_value(false), "release the finite input task stream without recycling it")
        ("hungarianMaxAgents", po::value<int>()->default_value(256), "capped Hungarian candidate-agent limit")
        ("hungarianMaxTasks", po::value<int>()->default_value(512), "capped Hungarian candidate-task limit")
        ("hungarianDistWeight", po::value<float>()->default_value(1.0f), "agent-to-pickup distance weight for capped Hungarian")
        ("hungarianTaskLengthWeight", po::value<float>()->default_value(1.0f), "task-internal path-length weight for capped Hungarian")
        ("debugTrace", po::value<bool>()->default_value(false), "write a compact per-timestep visualization trace")
        ("debugTraceOutput", po::value<std::string>()->default_value(""), "debug trace JSON output path; defaults to <output>.trace.json")
        ("debugTraceStart", po::value<int>()->default_value(0), "first timestep captured by debug trace")
        ("debugTraceEnd", po::value<int>()->default_value(-1), "last timestep captured by debug trace; -1 captures all")
        ("debugTraceAgentLimit", po::value<int>()->default_value(0), "maximum leading agents captured by debug trace; 0 captures all")
        ("refinementTimeLimit", po::value<int>()->default_value(0), "independent planner refinement limit in milliseconds; 0 uses planTimeLimit")
        ("commitWindow,w", po::value<int>()->default_value(1), "commit window");
    clock_t start_time = clock();
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 1;
    }

    po::notify(vm);

    boost::filesystem::path p(vm["inputFile"].as<std::string>());
    boost::filesystem::path dir = p.parent_path();
    std::string base_folder = dir.string();
    if (base_folder.size() > 0 && base_folder.back() != '/')
    {
        base_folder += "/";
    }

    int log_level = vm["logDetailLevel"].as<int>();
    if (log_level <= 1)
        log_level = 2; //info
    else if (log_level == 2)
        log_level = 3; //warning
    else
        log_level = 5; //fatal

    Logger *logger = new Logger(vm["logFile"].as<std::string>(),log_level);

    std::filesystem::path filepath(vm["output"].as<std::string>());
    if (filepath.parent_path().string().size() > 0 && !std::filesystem::is_directory(filepath.parent_path()))
    {
        logger->log_fatal("output directory does not exist",0);
        _exit(1);
    }


    Entry *planner = nullptr;

#ifdef PYTHON
#if PYTHON
        planner = new PyEntry();
#else
        planner = new Entry();
#endif
#endif

    auto input_json_file = vm["inputFile"].as<std::string>();
    json data;
    std::ifstream f(input_json_file);
    try
    {
        data = json::parse(f);
    }
    catch (json::parse_error error)
    {
        std::cerr << "Failed to load " << input_json_file << std::endl;
        std::cerr << "Message: " << error.what() << std::endl;
        exit(1);
    }

    auto map_path = read_param_json<std::string>(data, "mapFile");
    Grid grid(base_folder + map_path);

    planner->env->map_name = map_path.substr(map_path.find_last_of("/") + 1);


    string file_storage_path = vm["fileStoragePath"].as<std::string>();
    if (file_storage_path==""){
      char const* tmp = getenv("LORR_LARGE_FILE_STORAGE_PATH");
      if ( tmp != nullptr ) {
        file_storage_path = string(tmp);
      }
    }

    // check if the path exists;
    if (file_storage_path!="" &&!std::filesystem::exists(file_storage_path)){
      std::ostringstream stringStream;
      stringStream << "fileStoragePath (" << file_storage_path << ") is not valid";
      logger->log_warning(stringStream.str());
    }
    planner->env->file_storage_path = file_storage_path;

    planner->scheduler->set_use_traffic(vm["useTraffic"].as<bool>());
    planner->scheduler->set_new_only(vm["assignNew"].as<bool>());
    planner->scheduler->set_solver(vm["scheduleModel"].as<int>());
    DefaultPlanner::PortableGreedyHeapConfig heap_config;
    heap_config.dist_weight = vm["heapDistWeight"].as<float>();
    heap_config.max_assign_ratio = vm["heapMaxAssign"].as<float>();
    heap_config.reassign_enabled = vm["heapReassign"].as<bool>();
    heap_config.reassign_keep_bias = vm["heapKeepBias"].as<float>();
    heap_config.reassign_min_dist = vm["heapProtectDist"].as<int>();
    heap_config.rebuild_pct = vm["heapRebuildPct"].as<int>();
    heap_config.lns_pct = vm["heapLnsPct"].as<int>();
    heap_config.sort_k = vm["heapSortK"].as<int>();
    heap_config.flow_aware_spatial = vm["heapFlowAwareSpatial"].as<bool>();
    heap_config.spatial_cell_size = vm["heapSpatialCell"].as<int>();
    heap_config.spatial_candidate_limit = vm["heapSpatialCandidates"].as<int>();
    heap_config.traffic_weight = vm["heapTrafficWeight"].as<float>();
    heap_config.reassign_min_gain = vm["heapReassignMinGain"].as<float>();
    heap_config.future_flow_cell_size = vm["heapFutureFlowCell"].as<int>();
    heap_config.future_flow_weight = vm["heapFutureFlowWeight"].as<float>();
    heap_config.future_flow_hard_cap = vm["heapFutureFlowHardCap"].as<float>();
    heap_config.future_flow_regret_order = vm["heapFutureFlowRegret"].as<bool>();
    heap_config.opened_flow_zone_rows = vm["heapOpenedFlowZoneRows"].as<int>();
    heap_config.opened_flow_zone_cols = vm["heapOpenedFlowZoneCols"].as<int>();
    heap_config.opened_flow_penalty_weight = vm["heapOpenedFlowPenaltyWeight"].as<float>();
    planner->scheduler->set_heap_config(heap_config);
    DefaultPlanner::PortableTaskMatcherConfig matcher_config;
    matcher_config.dist_weight = vm["matcherDistWeight"].as<float>();
    matcher_config.candidate_top_k = vm["matcherTopK"].as<int>();
    matcher_config.max_matrix_elements = vm["matcherMaxMatrix"].as<int>();
    matcher_config.max_assign_ratio = vm["matcherMaxAssign"].as<float>();
    matcher_config.adaptive_assign_ratio = vm["matcherAdaptiveAssign"].as<bool>();
    matcher_config.wait_priority_weight = vm["matcherWaitPriorityWeight"].as<float>();
    matcher_config.wait_priority_threshold = vm["matcherWaitPriorityThreshold"].as<int>();
    matcher_config.use_traffic_cost = vm["matcherUseTraffic"].as<bool>();
    matcher_config.traffic_pressure_threshold = vm["matcherTrafficPressure"].as<float>();
    matcher_config.use_wait_heat = vm["matcherUseWaitHeat"].as<bool>();
    matcher_config.wait_heat_weight = vm["matcherWaitHeatWeight"].as<float>();
    matcher_config.wait_heat_pressure_threshold = vm["matcherWaitHeatPressure"].as<float>();
    matcher_config.guide_regret_weight = vm["matcherGuideRegretWeight"].as<float>();
    matcher_config.guide_regret_cap = vm["matcherGuideRegretCap"].as<int>();
    matcher_config.tail_rescue_weight = vm["matcherTailRescueWeight"].as<float>();
    matcher_config.tail_rescue_threshold = vm["matcherTailRescueThreshold"].as<int>();
    matcher_config.tail_rescue_cap = vm["matcherTailRescueCap"].as<int>();
    matcher_config.traffic_top_k = vm["matcherTrafficTopK"].as<int>();
    matcher_config.traffic_congestion_weight = vm["matcherTrafficCongestionWeight"].as<float>();
    matcher_config.reassign_enabled = vm["matcherReassign"].as<bool>();
    matcher_config.reassign_keep_bias = heap_config.reassign_keep_bias;
    matcher_config.reassign_min_dist = heap_config.reassign_min_dist;
    planner->scheduler->set_task_matcher_config(matcher_config);
    DefaultPlanner::PortableCappedHungarianConfig hungarian_config;
    hungarian_config.max_agents = vm["hungarianMaxAgents"].as<int>();
    hungarian_config.max_tasks = vm["hungarianMaxTasks"].as<int>();
    hungarian_config.dist_weight = vm["hungarianDistWeight"].as<float>();
    hungarian_config.task_length_weight = vm["hungarianTaskLengthWeight"].as<float>();
    planner->scheduler->set_capped_hungarian_config(hungarian_config);
    planner->planner->set_refinement_time_limit(vm["refinementTimeLimit"].as<int>());
    planner->commit_window = vm["commitWindow"].as<int>();

    ActionModel *model = new ActionModel(grid);
    model->set_logger(logger);

    int team_size = read_param_json<int>(data, "teamSize");

    std::vector<int> agents = read_int_vec(base_folder + read_param_json<std::string>(data, "agentFile"), team_size);
    std::vector<list<int>> tasks = read_int_vec(base_folder + read_param_json<std::string>(data, "taskFile"));
    if (agents.size() > tasks.size())
        logger->log_warning("Not enough tasks for robots (number of tasks < team size)");

    system_ptr = std::make_unique<BaseSystem>(grid, planner, agents, tasks, model);

    system_ptr->set_logger(logger);
    system_ptr->set_plan_time_limit(vm["planTimeLimit"].as<int>()*vm["commitWindow"].as<int>());//times commit window
    system_ptr->set_preprocess_time_limit(vm["preprocessTimeLimit"].as<int>());

    system_ptr->set_finite_task_stream(vm["finiteTaskStream"].as<bool>());
    system_ptr->set_num_tasks_reveal(read_param_json<float>(data, "numTasksReveal", 1));
    if (vm["debugTrace"].as<bool>()) {
        std::string trace_output = vm["debugTraceOutput"].as<std::string>();
        if (trace_output.empty()) trace_output = vm["output"].as<std::string>() + ".trace.json";
        system_ptr->configure_debug_trace(true, trace_output,
            vm["debugTraceStart"].as<int>(), vm["debugTraceEnd"].as<int>(),
            vm["debugTraceAgentLimit"].as<int>());
    }

    signal(SIGINT, sigint_handler);

    system_ptr->simulate(vm["simulationTime"].as<int>());


    system_ptr->saveResults(vm["output"].as<std::string>(),vm["outputScreen"].as<int>());

    delete model;
    delete logger;
    _exit(0);
}
