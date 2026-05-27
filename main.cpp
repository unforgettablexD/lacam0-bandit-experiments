#include <argparse/argparse.hpp>
#include <planner.hpp>

int main(int argc, char *argv[])
{
  // arguments parser
  auto program = argparse::ArgumentParser("lacam", "0.1.0");
  program.add_argument("-m", "--map").help("map file").required();
  program.add_argument("-i", "--scen").help("scenario file").default_value("");
  program.add_argument("-N", "--num")
      .help("number of agents")
      .scan<'d', int>()
      .required();
  program.add_argument("-s", "--seed")
      .help("seed")
      .scan<'d', int>()
      .default_value(0);
  program.add_argument("-v", "--verbose")
      .help("verbose")
      .scan<'d', int>()
      .default_value(0);
  program.add_argument("-t", "--time_limit_sec")
      .help("time limit sec")
      .scan<'g', float>()
      .default_value(3.0f);
  program.add_argument("-o", "--output")
      .help("output file")
      .default_value("./build/result.txt");
  program.add_argument("-l", "--log_short")
      .default_value(false)
      .implicit_value(true);

  // solver parameters
  program.add_argument("--anytime")
      .help("use anytime refinement by tree rewiring")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--no_dist_table_init")
      .help("disable to pre-compute distance tables with multi-threading")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--no_pibt_swap")
      .help("use vanilla PIBT as configuration generator")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--no_pibt_hindrance")
      .help("turn off the hindrance heuristic")
      .default_value(false)
      .implicit_value(true);
  // adaptive PIBT bandit flags (port from lacam branch)
  program.add_argument("--no_pibt_bandit")
      .help("disable PIBT candidate-ranking bandit (force arm0)")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--bandit_policy")
      .help("pibt bandit policy: ucb1 | thompson | epsilon_greedy | softmax | random_uniform")
      .default_value(std::string("ucb1"));
  program.add_argument("--bandit_epsilon")
      .help("epsilon for epsilon-greedy")
      .scan<'g', double>()
      .default_value(0.10);
  program.add_argument("--bandit_epsilon_final")
      .help("final epsilon for linear decay")
      .scan<'g', double>()
      .default_value(0.10);
  program.add_argument("--bandit_epsilon_decay_steps")
      .help("arm-pull count for epsilon linear decay (0 disables decay)")
      .scan<'d', int>()
      .default_value(0);
    // high-level bandit toggles
  program.add_argument("--no_order_bandit").default_value(false).implicit_value(true);
  program.add_argument("--no_branch_bandit").default_value(false).implicit_value(true);
  program.add_argument("--no_scheduler_bandit").default_value(false).implicit_value(true);
  program.add_argument("--no_random_bandit").default_value(false).implicit_value(true);
  program.add_argument("--no_dist_bandit").default_value(false).implicit_value(true);
    // compatibility flags (accepted for script parity)
  program.add_argument("--no_events_log").default_value(false).implicit_value(true);
  program.add_argument("--pibt_regret_trials").scan<'d', int>().default_value(1);
  program.add_argument("--bandit_hierarchy")
      .help("enable conditional hierarchy: pibt -> order|pibt -> branch|pibt,order")
      .default_value(false)
      .implicit_value(true);
  program.add_argument("--order_bandit_mode")
      .help("order bandit mode: coarse3 | agent_level")
      .default_value(std::string("coarse3"));
  program.add_argument("--order_agent_reward")
      .help("agent-level order reward: first_only | topk")
      .default_value(std::string("first_only"));
  program.add_argument("--pibt_rollouts")
      .help("number of Monte-Carlo PIBT rollouts per high-level expansion")
      .scan<'d', int>()
      .default_value(1);
  program.add_argument("--pibt_rollouts_after_goal")
      .help("rollouts used after first solution is found (<= pibt_rollouts)")
      .scan<'d', int>()
      .default_value(1);
  program.add_argument("--pibt_rollouts_early_margin")
      .help("early stop rollouts when best f improves over first by this margin")
      .scan<'d', int>()
      .default_value(0);
  program.add_argument("--reward_autoscale")
      .help("PIBT reward autoscale: off | zscore")
      .default_value(std::string("off"));
  program.add_argument("--reward_weight_learning")
      .help("PIBT reward weight learning: off | online_linear")
      .default_value(std::string("off"));
  program.add_argument("--reward_w_goal")
      .help("PIBT reward weight for goal bonus (-1 keeps default)")
      .scan<'g', double>()
      .default_value(-1.0);
  program.add_argument("--reward_w_delay")
      .help("PIBT reward weight for delay penalty (-1 keeps default)")
      .scan<'g', double>()
      .default_value(-1.0);
  program.add_argument("--reward_w_stay")
      .help("PIBT reward weight for stay penalty (-1 keeps default)")
      .scan<'g', double>()
      .default_value(-1.0);
  program.add_argument("--reward_w_leave")
      .help("PIBT reward weight for leave-goal penalty (-1 keeps default)")
      .scan<'g', double>()
      .default_value(-1.0);
  program.add_argument("--reward_w_occ")
      .help("PIBT reward weight for occupancy penalty (-1 keeps default)")
      .scan<'g', double>()
      .default_value(-1.0);
  program.add_argument("--reward_w_cong")
      .help("PIBT reward weight for local congestion penalty (-1 keeps default)")
      .scan<'g', double>()
      .default_value(-1.0);
  program.add_argument("--reward_w_noprog")
      .help("PIBT reward weight for no-progress penalty (-1 keeps default)")
      .scan<'g', double>()
      .default_value(-1.0);

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  // setup instance
  const auto verbose = program.get<int>("verbose");
  const auto time_limit_sec = program.get<float>("time_limit_sec");
  const auto scen_name = program.get<std::string>("scen");
  const auto seed = program.get<int>("seed");
  const auto map_name = program.get<std::string>("map");
  const auto output_name = program.get<std::string>("output");
  const auto log_short = program.get<bool>("log_short");
  const auto N = program.get<int>("num");
  const auto no_pibt_bandit = program.get<bool>("no_pibt_bandit");
  const auto no_order_bandit = program.get<bool>("no_order_bandit");
  const auto no_branch_bandit = program.get<bool>("no_branch_bandit");
  const auto no_scheduler_bandit = program.get<bool>("no_scheduler_bandit");
  const auto no_random_bandit = program.get<bool>("no_random_bandit");
  const auto no_dist_bandit = program.get<bool>("no_dist_bandit");
  const auto bandit_policy = program.get<std::string>("bandit_policy");
  const auto bandit_epsilon = program.get<double>("bandit_epsilon");
  const auto bandit_epsilon_final = program.get<double>("bandit_epsilon_final");
  const auto bandit_epsilon_decay_steps =
      program.get<int>("bandit_epsilon_decay_steps");
  const auto bandit_hierarchy = program.get<bool>("bandit_hierarchy");
  const auto order_bandit_mode = program.get<std::string>("order_bandit_mode");
  const auto order_agent_reward = program.get<std::string>("order_agent_reward");
    const auto pibt_rollouts = program.get<int>("pibt_rollouts");
  const auto pibt_rollouts_after_goal =
      program.get<int>("pibt_rollouts_after_goal");
  const auto pibt_rollouts_early_margin =
      program.get<int>("pibt_rollouts_early_margin");
  const auto reward_autoscale = program.get<std::string>("reward_autoscale");
  const auto reward_weight_learning =
      program.get<std::string>("reward_weight_learning");
    const auto reward_w_goal = program.get<double>("reward_w_goal");
    const auto reward_w_delay = program.get<double>("reward_w_delay");
    const auto reward_w_stay = program.get<double>("reward_w_stay");
    const auto reward_w_leave = program.get<double>("reward_w_leave");
    const auto reward_w_occ = program.get<double>("reward_w_occ");
    const auto reward_w_cong = program.get<double>("reward_w_cong");
    const auto reward_w_noprog = program.get<double>("reward_w_noprog");
  const auto no_events_log = program.get<bool>("no_events_log");
  const auto pibt_regret_trials = program.get<int>("pibt_regret_trials");
  const auto ins = scen_name.size() > 0 ? Instance(scen_name, map_name, N)
                                        : Instance(map_name, N, seed);
  if (!ins.is_valid(1)) return 1;

  // set hyper parameters
  DistTable::MULTI_THREAD_INIT = !program.get<bool>("no_dist_table_init");
  LaCAM::ANYTIME = program.get<bool>("anytime");
    LaCAM::PIBT_ROLLOUTS = std::max(1, pibt_rollouts);
  LaCAM::PIBT_ROLLOUTS_AFTER_GOAL =
      std::max(1, std::min(LaCAM::PIBT_ROLLOUTS, pibt_rollouts_after_goal));
  LaCAM::PIBT_ROLLOUTS_EARLY_STOP_MARGIN =
      std::max(0, pibt_rollouts_early_margin);

  // pibt
  PIBT::SWAP = !program.get<bool>("no_pibt_swap");
  PIBT::HINDRANCE = !program.get<bool>("no_pibt_hindrance");
  PIBT::set_bandit_config(!no_pibt_bandit, bandit_policy, bandit_epsilon,
                          bandit_epsilon_final, bandit_epsilon_decay_steps);
  PIBT::set_reward_config(reward_autoscale, reward_weight_learning);
    if (reward_w_goal >= 0.0 || reward_w_delay >= 0.0 || reward_w_stay >= 0.0 ||
            reward_w_leave >= 0.0 || reward_w_occ >= 0.0 || reward_w_cong >= 0.0 ||
            reward_w_noprog >= 0.0) {
        PIBT::set_reward_weights(
                reward_w_goal >= 0.0 ? reward_w_goal : 1.0,
                reward_w_delay >= 0.0 ? reward_w_delay : 1.0,
                reward_w_stay >= 0.0 ? reward_w_stay : 1.0,
                reward_w_leave >= 0.0 ? reward_w_leave : 1.0,
                reward_w_occ >= 0.0 ? reward_w_occ : 1.0,
                reward_w_cong >= 0.0 ? reward_w_cong : 1.0,
                reward_w_noprog >= 0.0 ? reward_w_noprog : 1.0);
    }
    PIBT::set_runtime_config(!no_events_log, pibt_regret_trials);
  LaCAM::set_bandit_config(!no_order_bandit, !no_branch_bandit,
                           !no_scheduler_bandit, !no_random_bandit,
                           bandit_hierarchy,
                           bandit_policy, bandit_epsilon,
                           bandit_epsilon_final, bandit_epsilon_decay_steps,
                           order_bandit_mode, order_agent_reward);
  DistTable::set_dist_bandit_config(!no_dist_bandit, bandit_policy,
                                    bandit_epsilon, bandit_epsilon_final,
                                    bandit_epsilon_decay_steps);

  // solve
  const auto deadline = Deadline(time_limit_sec * 1000);
  const auto solution = solve(ins, verbose - 1, &deadline, seed);
  const auto comp_time_ms = deadline.elapsed_ms();

  // failure
  if (solution.empty()) info(1, verbose, &deadline, "failed to solve");

  // check feasibility
  if (!is_feasible_solution(ins, solution, verbose)) {
    info(0, verbose, &deadline, "invalid solution");
    return 1;
  }

  // post processing
  print_stats(verbose, &deadline, ins, solution, comp_time_ms);
  make_log(ins, solution, output_name, comp_time_ms, map_name, seed, log_short);
  return 0;
}
