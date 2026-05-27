/*
 * implementation of PIBT
 *
 * references:
 * Priority Inheritance with Backtracking for Iterative Multi-agent Path
 * Finding. Keisuke Okumura, Manao Machida, Xavier Défago & Yasumasa Tamura.
 * Artificial Intelligence (AIJ). 2022.
 *
 * swap:
 * Improving lacam for scalable eventually optimal multi-agent pathfinding.
 * Keisuke Okumura.
 * IJCAI. 2023.
 *
 * hindrance:
 * Lightweight and Effective Preference Construction in PIBT for Large-Scale
 * Multi-Agent Pathfinding
 * Keisuke Okumura & Hiroki Nagai.
 * SoCS. 2025.
 */
#pragma once
#include <array>
#include "dist_table.hpp"
#include "graph.hpp"
#include "instance.hpp"
#include "utils.hpp"

// dist, hindrance, tie
using PIBTHeuristic = std::tuple<int, int, float>;

struct PIBT {
  const Instance *ins;
  std::mt19937 MT;
  std::uniform_real_distribution<float> rrd;  // random, real distribution

  // solver utils
  const int N;  // number of agents
  const int V_size;
  DistTable *D;
  const Deadline *deadline;

  // specific to PIBT
  const int NO_AGENT;
  std::vector<int> occupied_now;                 // for quick collision checking
  std::vector<int> occupied_next;                // for quick collision checking
  std::vector<std::array<Vertex *, 5> > C_next;  // next location candidates
  std::array<PIBTHeuristic, 5> C_cost;           // action cost
  std::vector<std::array<int, 5> > C_indices;    // action index

  // hyper parameters
  static bool SWAP;
  static bool HINDRANCE;
  static bool USE_PIBT_BANDIT;
  static bool FORCE_PIBT_ARM;
  static int FORCED_PIBT_ARM;
  static int LAST_PIBT_ARM;
  static std::string BANDIT_POLICY;
  static double BANDIT_EPSILON;
  static double BANDIT_EPSILON_FINAL;
  static int BANDIT_EPSILON_DECAY_STEPS;
  static std::string REWARD_AUTOSCALE_MODE;   // off | zscore
  static std::string REWARD_WEIGHT_LEARNING;  // off | online_linear
  static bool EVENTS_LOG_ENABLED;
  static int REGRET_TRIALS;

  static void set_bandit_config(bool use_pibt_bandit,
                                const std::string &bandit_policy,
                                double bandit_epsilon,
                                double bandit_epsilon_final,
                                int bandit_epsilon_decay_steps);
  static void set_reward_config(const std::string &reward_autoscale_mode,
                                const std::string &reward_weight_learning);
  static void set_reward_weights(double w_goal, double w_delay, double w_stay,
                                 double w_leave, double w_occ,
                                 double w_congestion, double w_no_progress);
  static void set_runtime_config(bool events_log_enabled, int regret_trials);
  static void set_forced_pibt_arm(int arm);  // arm<0 disables forcing

  PIBT(const Instance *_ins, DistTable *_D, const Deadline *_deadline = nullptr,
       int seed = 0);
  ~PIBT();

  bool set_new_config(const Config &Q_from, Config &Q_to,
                      const std::vector<int> &order);
  bool funcPIBT(const int i, const Config &Q_from, Config &Q_to);

  int is_swap_required_and_possible(const int ai, const Config &Q_from,
                                    Config &Q_to, Vertex *v_i_target);
  bool is_swap_required(const int pusher, const int puller,
                        Vertex *v_pusher_origin, Vertex *v_puller_origin);
  bool is_swap_possible(Vertex *v_pusher_origin, Vertex *v_puller_origin);
};
