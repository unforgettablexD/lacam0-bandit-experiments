/*
 * distance table with lazy evaluation, using BFS
 */
#pragma once

#include "graph.hpp"
#include "instance.hpp"
#include "utils.hpp"

struct DistTable {
  const int K;  // number of vertices
  std::vector<std::vector<int>>
      table;  // distance table, index: agent-id & vertex-id
  std::vector<std::queue<Vertex *>> OPEN;  // search queue

  static bool MULTI_THREAD_INIT;
  static bool USE_DIST_BANDIT;
  static std::string DIST_BANDIT_POLICY;
  static double DIST_BANDIT_EPSILON;
  static double DIST_BANDIT_EPSILON_FINAL;
  static int DIST_BANDIT_EPSILON_DECAY_STEPS;

  static void set_dist_bandit_config(bool use_dist_bandit,
                                     const std::string &bandit_policy,
                                     double bandit_epsilon,
                                     double bandit_epsilon_final,
                                     int bandit_epsilon_decay_steps);

  int get(const int i, const int v_id);   // agent, vertex-id
  int get(const int i, const Vertex *v);  // agent, vertex

  DistTable(const Instance &ins);
  DistTable(const Instance *ins);

  void setup(const Instance *ins);  // initialization
};
