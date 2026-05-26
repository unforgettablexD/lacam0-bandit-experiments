#include "../include/pibt.hpp"

bool PIBT::SWAP = true;
bool PIBT::HINDRANCE = true;
bool PIBT::USE_PIBT_BANDIT = true;
bool PIBT::FORCE_PIBT_ARM = false;
int PIBT::FORCED_PIBT_ARM = 0;
int PIBT::LAST_PIBT_ARM = 0;
std::string PIBT::BANDIT_POLICY = "ucb1";
double PIBT::BANDIT_EPSILON = 0.10;
double PIBT::BANDIT_EPSILON_FINAL = 0.10;
int PIBT::BANDIT_EPSILON_DECAY_STEPS = 0;
std::string PIBT::REWARD_AUTOSCALE_MODE = "off";
std::string PIBT::REWARD_WEIGHT_LEARNING = "off";

namespace {
constexpr int BANDIT_ARM_COUNT = 3;
std::array<int, BANDIT_ARM_COUNT> PIBT_ARM_PULLS = {0, 0, 0};
std::array<double, BANDIT_ARM_COUNT> PIBT_ARM_REWARDS = {0.0, 0.0, 0.0};

struct RunningStat {
  int n = 0;
  double mean = 0.0;
  double m2 = 0.0;
  void update(double x) {
    n += 1;
    const double d = x - mean;
    mean += d / n;
    const double d2 = x - mean;
    m2 += d * d2;
  }
  double stddev() const { return (n > 1) ? std::sqrt(m2 / (n - 1)) : 1.0; }
  double z(double x) const {
    const double s = std::max(1e-6, stddev());
    return (x - mean) / s;
  }
};
RunningStat ST_GOAL, ST_DELAY, ST_STAY, ST_LEAVE, ST_OCC;
double W_GOAL = 1.0, W_DELAY = 1.0, W_STAY = 1.0, W_LEAVE = 1.0, W_OCC = 1.0;

double squash_reward(const double raw_reward) { return std::tanh(raw_reward); }

int pick_arm(std::mt19937 &rng)
{
  if (!PIBT::USE_PIBT_BANDIT) return 0;
  for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
    if (PIBT_ARM_PULLS[a] == 0) return a;
  }

  const auto policy = PIBT::BANDIT_POLICY;
  if (policy == "epsilon_greedy" || policy == "eps" || policy == "epsilon") {
    int total = 0;
    for (const auto p : PIBT_ARM_PULLS) total += p;
    double epsilon = PIBT::BANDIT_EPSILON;
    if (PIBT::BANDIT_EPSILON_DECAY_STEPS > 0) {
      const double t = std::min(1.0, static_cast<double>(total) /
                                         static_cast<double>(PIBT::BANDIT_EPSILON_DECAY_STEPS));
      epsilon = PIBT::BANDIT_EPSILON +
                (PIBT::BANDIT_EPSILON_FINAL - PIBT::BANDIT_EPSILON) * t;
    }
    std::uniform_real_distribution<double> U01(0.0, 1.0);
    std::uniform_int_distribution<int> UArm(0, BANDIT_ARM_COUNT - 1);
    if (U01(rng) < epsilon) return UArm(rng);
    int best = 0;
    double best_mean = -1e18;
    for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
      const auto mean = PIBT_ARM_REWARDS[a] / PIBT_ARM_PULLS[a];
      if (mean > best_mean) {
        best_mean = mean;
        best = a;
      }
    }
    return best;
  }

  if (policy == "thompson" || policy == "ts") {
    int best = 0;
    double best_sample = -1e18;
    for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
      const auto mean = PIBT_ARM_REWARDS[a] / PIBT_ARM_PULLS[a];
      const auto sigma = 1.0 / std::sqrt((double)PIBT_ARM_PULLS[a]);
      std::normal_distribution<double> N(mean, sigma);
      const auto sample = N(rng);
      if (sample > best_sample) {
        best_sample = sample;
        best = a;
      }
    }
    return best;
  }

  if (policy == "random_uniform" || policy == "random" || policy == "uniform_random") {
    std::uniform_int_distribution<int> UArm(0, BANDIT_ARM_COUNT - 1);
    return UArm(rng);
  }

  // default UCB1
  int total = 0;
  for (const auto p : PIBT_ARM_PULLS) total += p;
  int best = 0;
  double best_score = -1e18;
  for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
    const auto mean = PIBT_ARM_REWARDS[a] / PIBT_ARM_PULLS[a];
    const auto bonus = std::sqrt(2.0 * std::log((double)total) / PIBT_ARM_PULLS[a]);
    const auto score = mean + bonus;
    if (score > best_score) {
      best_score = score;
      best = a;
    }
  }
  return best;
}

void update_arm(const int arm, const double reward)
{
  PIBT_ARM_PULLS[arm] += 1;
  PIBT_ARM_REWARDS[arm] += reward;
}
}  // namespace

void PIBT::set_bandit_config(bool use_pibt_bandit,
                             const std::string &bandit_policy,
                             double bandit_epsilon,
                             double bandit_epsilon_final,
                             int bandit_epsilon_decay_steps)
{
  USE_PIBT_BANDIT = use_pibt_bandit;
  BANDIT_POLICY = bandit_policy.empty() ? "ucb1" : bandit_policy;
  BANDIT_EPSILON = std::max(0.0, std::min(1.0, bandit_epsilon));
  BANDIT_EPSILON_FINAL = std::max(0.0, std::min(1.0, bandit_epsilon_final));
  BANDIT_EPSILON_DECAY_STEPS = std::max(0, bandit_epsilon_decay_steps);
}

void PIBT::set_forced_pibt_arm(int arm)
{
  if (arm < 0) {
    FORCE_PIBT_ARM = false;
    return;
  }
  FORCE_PIBT_ARM = true;
  FORCED_PIBT_ARM = std::max(0, std::min(2, arm));
}

void PIBT::set_reward_config(const std::string &reward_autoscale_mode,
                             const std::string &reward_weight_learning)
{
  REWARD_AUTOSCALE_MODE = reward_autoscale_mode.empty() ? "off" : reward_autoscale_mode;
  REWARD_WEIGHT_LEARNING =
      reward_weight_learning.empty() ? "off" : reward_weight_learning;
}

PIBT::PIBT(const Instance *_ins, DistTable *_D, int seed)
    : ins(_ins),
      MT(seed),
      rrd(0, 1),
      N(ins->N),
      V_size(ins->G.size()),
      D(_D),
      NO_AGENT(N),
      occupied_now(V_size, NO_AGENT),
      occupied_next(V_size, NO_AGENT),
      C_next(N),
      C_indices(N)
{
}

PIBT::~PIBT() {}

bool PIBT::set_new_config(const Config &Q_from, Config &Q_to,
                          const std::vector<int> &order)
{
  bool success = true;
  // setup cache & constraints check
  for (auto i = 0; i < N; ++i) {
    // set occupied now
    occupied_now[Q_from[i]->id] = i;

    // set occupied next
    if (Q_to[i] != nullptr) {
      // vertex collision
      if (occupied_next[Q_to[i]->id] != NO_AGENT) {
        success = false;
        break;
      }
      // swap collision
      auto j = occupied_now[Q_to[i]->id];
      if (j != NO_AGENT && j != i && Q_to[j] == Q_from[i]) {
        success = false;
        break;
      }
      occupied_next[Q_to[i]->id] = i;
    }
  }

  if (success) {
    for (auto i : order) {
      if (Q_to[i] == nullptr && !funcPIBT(i, Q_from, Q_to)) {
        success = false;
        break;
      }
    }
  }

  // cleanup
  for (auto i = 0; i < N; ++i) {
    occupied_now[Q_from[i]->id] = NO_AGENT;
    if (Q_to[i] != nullptr) occupied_next[Q_to[i]->id] = NO_AGENT;
  }

  return success;
}

bool PIBT::funcPIBT(const int i, const Config &Q_from, Config &Q_to)
{
  const auto K = Q_from[i]->neighbors.size();
  const auto selected_arm = FORCE_PIBT_ARM ? FORCED_PIBT_ARM : pick_arm(MT);
  LAST_PIBT_ARM = selected_arm;
  const auto d_now = D->get(i, Q_from[i]);
  const bool at_goal_now = (d_now == 0);

  // hindrance preparation
  int num_neighbor_agents = 0;
  static std::array<int, 4> neighbor_agents;
  if (HINDRANCE) {
    for (auto u : Q_from[i]->neighbors) {
      if (occupied_now[u->id] != NO_AGENT) {
        neighbor_agents[num_neighbor_agents] = occupied_now[u->id];
        ++num_neighbor_agents;
      }
    }
  }

  auto get_successor_cost = [&](Vertex *u, int mode, bool swap = false) {
    auto e = rrd(MT);
    if (swap) return std::make_tuple(-D->get(i, u), 0, e);

    int hindrance = 0;
    if (HINDRANCE) {
      for (auto k = 0; k < num_neighbor_agents; ++k) {
        auto &&j = neighbor_agents[k];
        if (Q_from[j] != u && D->get(j, u) < D->get(j, Q_from[j])) {
          hindrance += 1;
        }
      }
    }
    int regret = 0;
    for (auto nb : u->neighbors) {
      const auto j = occupied_now[nb->id];
      if (j != NO_AGENT && j != i) {
        if (D->get(j, u) < D->get(j, Q_from[j])) regret += 1;
      }
    }
    if (mode == 0) return std::make_tuple(D->get(i, u), hindrance, e);
    if (mode == 1) return std::make_tuple(D->get(i, u), hindrance + 1, e);
    return std::make_tuple(D->get(i, u), hindrance + regret + 1, e);
  };

  // set C_next
  for (size_t k = 0; k <= K; ++k) {
    auto u = Q_from[i]->actions[k];
    C_next[i][k] = u;
    C_cost[k] = get_successor_cost(u, selected_arm);
  }
  // sort, note: K + 1 is sufficient
  std::iota(C_indices[i].begin(), C_indices[i].begin() + K + 1, 0);
  std::sort(C_indices[i].begin(), C_indices[i].begin() + K + 1,
            [&](const int k, const int l) { return C_cost[k] < C_cost[l]; });

  // emulate swap
  const auto swap_agent = is_swap_required_and_possible(
      i, Q_from, Q_to, C_next[i][C_indices[i][0]]);
  if (swap_agent != NO_AGENT) {
    // recompute action cost
    for (size_t k = 0; k < K + 1; ++k) {
      C_cost[k] = get_successor_cost(C_next[i][k], selected_arm, true);
      C_indices[i][k] = k;
    }
    std::sort(C_indices[i].begin(), C_indices[i].begin() + K + 1,
              [&](const int k, const int l) { return C_cost[k] < C_cost[l]; });
  }
  auto swap_operation = [&]() {
    if (swap_agent != NO_AGENT &&                 // swap_agent exists
        Q_to[swap_agent] == nullptr &&            // not decided
        occupied_next[Q_from[i]->id] == NO_AGENT  // free
    ) {
      // pull swap_agent
      occupied_next[Q_from[i]->id] = swap_agent;
      Q_to[swap_agent] = Q_from[i];
    }
  };

  // main loop
  for (size_t k = 0; k < K + 1; ++k) {
    auto u_idx = C_indices[i][k];
    auto u = C_next[i][u_idx];

    // avoid vertex conflicts
    if (occupied_next[u->id] != NO_AGENT) continue;

    const auto j = occupied_now[u->id];

    // avoid swap conflicts with constraints
    if (j != NO_AGENT && Q_to[j] == Q_from[i]) continue;

    // reserve next location
    occupied_next[u->id] = i;
    Q_to[i] = u;
    const auto d_next = D->get(i, u);
    const bool stay_move = (u == Q_from[i]);
    const bool at_goal_next = (d_next == 0);
    const int ideal_next = std::max(0, d_now - 1);
    const int step_delay = std::max(0, d_next - ideal_next);
    const double delay_penalty = static_cast<double>(step_delay);
    const double stay_penalty = (!at_goal_now && stay_move) ? 0.25 : 0.0;
    const double leave_goal_penalty = (at_goal_now && !at_goal_next) ? 1.5 : 0.0;
    const double occupancy_penalty = (j != NO_AGENT && !stay_move) ? 0.20 : 0.0;
    const double goal_bonus = (!at_goal_now && at_goal_next) ? 0.50 : 0.0;
    ST_GOAL.update(goal_bonus);
    ST_DELAY.update(delay_penalty);
    ST_STAY.update(stay_penalty);
    ST_LEAVE.update(leave_goal_penalty);
    ST_OCC.update(occupancy_penalty);
    const bool zscore = (REWARD_AUTOSCALE_MODE == "zscore");
    const double goal_term = zscore ? ST_GOAL.z(goal_bonus) : goal_bonus;
    const double delay_term = zscore ? ST_DELAY.z(delay_penalty) : delay_penalty;
    const double stay_term = zscore ? ST_STAY.z(stay_penalty) : stay_penalty;
    const double leave_term = zscore ? ST_LEAVE.z(leave_goal_penalty) : leave_goal_penalty;
    const double occ_term = zscore ? ST_OCC.z(occupancy_penalty) : occupancy_penalty;
    const double raw_reward =
        W_GOAL * goal_term - W_DELAY * delay_term - W_STAY * stay_term -
        W_LEAVE * leave_term - W_OCC * occ_term;
    const double immediate_reward = squash_reward(raw_reward);

    if (REWARD_WEIGHT_LEARNING == "online_linear") {
      // simple online adaptation: predict step quality (+1 progress, -1 no progress)
      const double y = (d_next < d_now) ? 1.0 : -1.0;
      const double err = y - raw_reward;
      constexpr double lr = 0.01;
      W_GOAL = std::max(0.0, std::min(3.0, W_GOAL + lr * err * goal_term));
      W_DELAY = std::max(0.0, std::min(3.0, W_DELAY - lr * err * delay_term));
      W_STAY = std::max(0.0, std::min(3.0, W_STAY - lr * err * stay_term));
      W_LEAVE = std::max(0.0, std::min(3.0, W_LEAVE - lr * err * leave_term));
      W_OCC = std::max(0.0, std::min(3.0, W_OCC - lr * err * occ_term));
    }

    // priority inheritance
    if (j != NO_AGENT && u != Q_from[i] && Q_to[j] == nullptr &&
        !funcPIBT(j, Q_from, Q_to)) {
      update_arm(selected_arm, immediate_reward - 1.0);
      continue;
    }

    // success to plan next one step
    update_arm(selected_arm, immediate_reward + 0.2);
    if (k == 0) swap_operation();
    return true;
  }

  // failed to secure node
  occupied_next[Q_from[i]->id] = i;
  Q_to[i] = Q_from[i];
  update_arm(selected_arm, -1.0);
  return false;
}

int PIBT::is_swap_required_and_possible(const int i, const Config &Q_from,
                                        Config &Q_to, Vertex *v_i_target)
{
  if (!SWAP) return NO_AGENT;
  // agent-j occupying the desired vertex for agent-i
  const auto j = occupied_now[v_i_target->id];
  if (j != NO_AGENT && j != i &&  // j exists
      Q_to[j] == nullptr &&       // j does not decide next location
      is_swap_required(i, j, Q_from[i], Q_from[j]) &&  // swap required
      is_swap_possible(Q_from[j], Q_from[i])           // swap possible
  ) {
    return j;
  }

  // for clear operation, c.f., push & swap
  if (v_i_target != Q_from[i]) {
    for (auto u : Q_from[i]->neighbors) {
      const auto k = occupied_now[u->id];
      if (k != NO_AGENT &&            // k exists
          v_i_target != Q_from[k] &&  // this is for clear operation
          is_swap_required(k, i, Q_from[i],
                           v_i_target) &&  // emulating from one step ahead
          is_swap_possible(v_i_target, Q_from[i])) {
        return k;
      }
    }
  }
  return NO_AGENT;
}

bool PIBT::is_swap_required(const int pusher, const int puller,
                            Vertex *v_pusher_origin, Vertex *v_puller_origin)
{
  auto v_pusher = v_pusher_origin;
  auto v_puller = v_puller_origin;
  Vertex *tmp = nullptr;
  while (D->get(pusher, v_puller) < D->get(pusher, v_pusher)) {
    auto n = v_puller->neighbors.size();
    // remove agents who need not to move
    for (auto u : v_puller->neighbors) {
      const auto i = occupied_now[u->id];
      if (u == v_pusher ||
          (u->neighbors.size() == 1 && i != NO_AGENT && ins->goals[i] == u)) {
        --n;
      } else {
        tmp = u;
      }
    }
    if (n >= 2) return false;  // able to swap at v_l
    if (n <= 0) break;
    v_pusher = v_puller;
    v_puller = tmp;
  }

  return (D->get(puller, v_pusher) < D->get(puller, v_puller)) &&
         (D->get(pusher, v_pusher) == 0 ||
          D->get(pusher, v_puller) < D->get(pusher, v_pusher));
}

bool PIBT::is_swap_possible(Vertex *v_pusher_origin, Vertex *v_puller_origin)
{
  // simulate pull
  auto v_pusher = v_pusher_origin;
  auto v_puller = v_puller_origin;
  Vertex *tmp = nullptr;
  while (v_puller != v_pusher_origin) {  // avoid loop
    auto n = v_puller->neighbors.size();
    for (auto u : v_puller->neighbors) {
      const auto i = occupied_now[u->id];
      if (u == v_pusher ||
          (u->neighbors.size() == 1 && i != NO_AGENT && ins->goals[i] == u)) {
        --n;
      } else {
        tmp = u;
      }
    }
    if (n >= 2) return true;  // able to swap at v_next
    if (n <= 0) return false;
    v_pusher = v_puller;
    v_puller = tmp;
  }
  return false;
}
