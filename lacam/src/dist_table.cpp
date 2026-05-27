#include "../include/dist_table.hpp"

bool DistTable::MULTI_THREAD_INIT = true;
bool DistTable::USE_DIST_BANDIT = true;
std::string DistTable::DIST_BANDIT_POLICY = "ucb1";
double DistTable::DIST_BANDIT_EPSILON = 0.10;
double DistTable::DIST_BANDIT_EPSILON_FINAL = 0.10;
int DistTable::DIST_BANDIT_EPSILON_DECAY_STEPS = 0;

namespace {
constexpr int DIST_ARM_COUNT = 3;
std::array<int, DIST_ARM_COUNT> DIST_PULLS = {0, 0, 0};
std::array<double, DIST_ARM_COUNT> DIST_REWARDS = {0.0, 0.0, 0.0};
std::mt19937 DIST_RNG(4242);

int pick_dist_arm()
{
  if (!DistTable::USE_DIST_BANDIT) return 0;
  for (int a = 0; a < DIST_ARM_COUNT; ++a) {
    if (DIST_PULLS[a] == 0) return a;
  }

  if (DistTable::DIST_BANDIT_POLICY == "epsilon_greedy" ||
      DistTable::DIST_BANDIT_POLICY == "eps" ||
      DistTable::DIST_BANDIT_POLICY == "epsilon") {
    int total = 0;
    for (const auto p : DIST_PULLS) total += p;
    double eps = DistTable::DIST_BANDIT_EPSILON;
    if (DistTable::DIST_BANDIT_EPSILON_DECAY_STEPS > 0) {
      const double t = std::min(
          1.0, static_cast<double>(total) /
                   static_cast<double>(DistTable::DIST_BANDIT_EPSILON_DECAY_STEPS));
      eps = DistTable::DIST_BANDIT_EPSILON +
            (DistTable::DIST_BANDIT_EPSILON_FINAL - DistTable::DIST_BANDIT_EPSILON) * t;
    }
    std::uniform_real_distribution<double> U01(0.0, 1.0);
    std::uniform_int_distribution<int> UArm(0, DIST_ARM_COUNT - 1);
    if (U01(DIST_RNG) < eps) return UArm(DIST_RNG);
    int best = 0;
    double best_mean = -1e18;
    for (int a = 0; a < DIST_ARM_COUNT; ++a) {
      const auto mean = DIST_REWARDS[a] / DIST_PULLS[a];
      if (mean > best_mean) {
        best_mean = mean;
        best = a;
      }
    }
    return best;
  }

  if (DistTable::DIST_BANDIT_POLICY == "thompson" ||
      DistTable::DIST_BANDIT_POLICY == "ts") {
    int best = 0;
    double best_sample = -1e18;
    for (int a = 0; a < DIST_ARM_COUNT; ++a) {
      const auto mean = DIST_REWARDS[a] / DIST_PULLS[a];
      const auto sigma = 1.0 / std::sqrt((double)DIST_PULLS[a]);
      std::normal_distribution<double> N(mean, sigma);
      const auto s = N(DIST_RNG);
      if (s > best_sample) {
        best_sample = s;
        best = a;
      }
    }
    return best;
  }

  if (DistTable::DIST_BANDIT_POLICY == "random_uniform" ||
      DistTable::DIST_BANDIT_POLICY == "random" ||
      DistTable::DIST_BANDIT_POLICY == "uniform_random") {
    std::uniform_int_distribution<int> UArm(0, DIST_ARM_COUNT - 1);
    return UArm(DIST_RNG);
  }

  int total = 0;
  for (const auto p : DIST_PULLS) total += p;
  int best = 0;
  double best_score = -1e18;
  for (int a = 0; a < DIST_ARM_COUNT; ++a) {
    const auto mean = DIST_REWARDS[a] / DIST_PULLS[a];
    const auto bonus = std::sqrt(2.0 * std::log((double)total) / DIST_PULLS[a]);
    const auto score = mean + bonus;
    if (score > best_score) {
      best_score = score;
      best = a;
    }
  }
  return best;
}

void update_dist_arm(const int arm, const double reward)
{
  if (!DistTable::USE_DIST_BANDIT) return;
  DIST_PULLS[arm] += 1;
  DIST_REWARDS[arm] += reward;
}
}  // namespace

void DistTable::set_dist_bandit_config(bool use_dist_bandit,
                                       const std::string &bandit_policy,
                                       double bandit_epsilon,
                                       double bandit_epsilon_final,
                                       int bandit_epsilon_decay_steps)
{
  USE_DIST_BANDIT = use_dist_bandit;
  DIST_BANDIT_POLICY = bandit_policy.empty() ? "ucb1" : bandit_policy;
  DIST_BANDIT_EPSILON = std::max(0.0, std::min(1.0, bandit_epsilon));
  DIST_BANDIT_EPSILON_FINAL = std::max(0.0, std::min(1.0, bandit_epsilon_final));
  DIST_BANDIT_EPSILON_DECAY_STEPS = std::max(0, bandit_epsilon_decay_steps);
}

DistTable::DistTable(const Instance &ins)
    : K(ins.G.V.size()), table(ins.N, std::vector<int>(K, K))
{
  setup(&ins);
}

DistTable::DistTable(const Instance *ins)
    : K(ins->G.V.size()), table(ins->N, std::vector<int>(K, K))
{
  setup(ins);
}

void DistTable::setup(const Instance *ins)
{
  const int arm = pick_dist_arm();
  if (MULTI_THREAD_INIT && arm == 0) {
    auto bfs = [&](const int i) {
      auto g_i = ins->goals[i];
      auto Q = std::queue<Vertex *>({g_i});
      table[i][g_i->id] = 0;
      while (!Q.empty()) {
        auto n = Q.front();
        Q.pop();
        const int d_n = table[i][n->id];
        for (auto &m : n->neighbors) {
          const int d_m = table[i][m->id];
          if (d_n + 1 >= d_m) continue;
          table[i][m->id] = d_n + 1;
          Q.push(m);
        }
      }
    };

    auto pool = std::vector<std::future<void>>();
    for (size_t i = 0; i < ins->N; ++i) {
      pool.emplace_back(std::async(std::launch::async, bfs, i));
    }
  } else {
    // lazy BFS
    for (size_t i = 0; i < ins->N; ++i) {
      OPEN.push_back(std::queue<Vertex *>());
      auto n = ins->goals[i];
      OPEN[i].push(n);
      table[i][n->id] = 0;
    }
    if (arm == 1) {
      const int k = std::min<int>(ins->N, 8);
      for (int i = 0; i < k; ++i) (void)get(i, ins->starts[i]);
    } else if (arm == 2) {
      const int pop_budget = 32;
      for (size_t i = 0; i < ins->N; ++i) {
        int c = 0;
        while (!OPEN[i].empty() && c < pop_budget) {
          auto n = OPEN[i].front();
          OPEN[i].pop();
          const int d_n = table[i][n->id];
          for (auto &m : n->neighbors) {
            const int d_m = table[i][m->id];
            if (d_n + 1 >= d_m) continue;
            table[i][m->id] = d_n + 1;
            OPEN[i].push(m);
          }
          ++c;
        }
      }
    }
  }

  double known = 0.0;
  for (size_t i = 0; i < ins->N; ++i) {
    known += (table[i][ins->starts[i]->id] < K) ? 1.0 : 0.0;
  }
  update_dist_arm(arm, known / std::max<size_t>(1, ins->N));
}

int DistTable::get(const int i, const int v_id)
{
  if (table[i][v_id] < K) return table[i][v_id];

  /*
   * BFS with lazy evaluation
   * c.f., Reverse Resumable A*
   * https://www.aaai.org/Papers/AIIDE/2005/AIIDE05-020.pdf
   *
   * sidenote:
   * tested RRA* but lazy BFS was much better in performance
   */

  while (!OPEN[i].empty()) {
    auto &&n = OPEN[i].front();
    OPEN[i].pop();
    const int d_n = table[i][n->id];
    for (auto &&m : n->neighbors) {
      const int d_m = table[i][m->id];
      if (d_n + 1 >= d_m) continue;
      table[i][m->id] = d_n + 1;
      OPEN[i].push(m);
    }
    if (n->id == v_id) return d_n;
  }
  return K;
}

int DistTable::get(const int i, const Vertex *v) { return get(i, v->id); }
