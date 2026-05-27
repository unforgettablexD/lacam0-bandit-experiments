#include "../include/pibt.hpp"

#include <filesystem>

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
bool PIBT::EVENTS_LOG_ENABLED = true;
int PIBT::REGRET_TRIALS = 1;

namespace {
constexpr int BANDIT_ARM_COUNT = 3;
std::array<int, BANDIT_ARM_COUNT> PIBT_ARM_PULLS = {0, 0, 0};
std::array<double, BANDIT_ARM_COUNT> PIBT_ARM_REWARDS = {0.0, 0.0, 0.0};
std::ofstream PIBT_EVENTS_LOG;
bool PIBT_EVENTS_LOG_READY = false;
long long PIBT_EVENT_SEQ = 0;

void log_pibt_event(const int arm, const double reward)
{
  if (!PIBT::EVENTS_LOG_ENABLED) return;

  const std::string path = "./build/events.csv";
  if (!PIBT_EVENTS_LOG_READY) {
    const bool write_header = !std::filesystem::exists(path) ||
                              std::filesystem::file_size(path) == 0;
    PIBT_EVENTS_LOG.open(path, std::ios::out | std::ios::app);
    if (!PIBT_EVENTS_LOG.is_open()) return;
    if (write_header) {
      PIBT_EVENTS_LOG
          << "seq,module,arm,reward,pulls0,pulls1,pulls2,rewards0,rewards1,rewards2,policy,use_bandit,regret_trials\n";
    }
    PIBT_EVENTS_LOG_READY = true;
  }

  ++PIBT_EVENT_SEQ;
  PIBT_EVENTS_LOG << PIBT_EVENT_SEQ << ",pibt," << arm << "," << reward
                  << "," << PIBT_ARM_PULLS[0] << "," << PIBT_ARM_PULLS[1]
                  << "," << PIBT_ARM_PULLS[2] << "," << PIBT_ARM_REWARDS[0]
                  << "," << PIBT_ARM_REWARDS[1] << "," << PIBT_ARM_REWARDS[2]
                  << "," << PIBT::BANDIT_POLICY << ","
                  << (PIBT::USE_PIBT_BANDIT ? 1 : 0) << ","
                  << PIBT::REGRET_TRIALS << "\n";
}

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
RunningStat ST_GOAL, ST_DELAY, ST_STAY, ST_LEAVE, ST_OCC, ST_CONG, ST_NOPROG;
double W_GOAL = 1.0, W_DELAY = 1.0, W_STAY = 1.0, W_LEAVE = 1.0, W_OCC = 1.0,
       W_CONG = 1.0, W_NOPROG = 1.0;

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

  if (policy == "softmax" || policy == "boltzmann") {
    const double temp = std::max(0.02, PIBT::BANDIT_EPSILON);
    std::array<double, BANDIT_ARM_COUNT> logits = {0.0, 0.0, 0.0};
    double max_logit = -1e18;
    for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
      const auto mean = PIBT_ARM_REWARDS[a] / std::max(1, PIBT_ARM_PULLS[a]);
      logits[a] = mean / temp;
      max_logit = std::max(max_logit, logits[a]);
    }
    std::array<double, BANDIT_ARM_COUNT> probs = {0.0, 0.0, 0.0};
    double sum_exp = 0.0;
    for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
      probs[a] = std::exp(logits[a] - max_logit);
      sum_exp += probs[a];
    }
    if (sum_exp <= 0.0) {
      std::uniform_int_distribution<int> UArm(0, BANDIT_ARM_COUNT - 1);
      return UArm(rng);
    }
    for (int a = 0; a < BANDIT_ARM_COUNT; ++a) probs[a] /= sum_exp;
    std::discrete_distribution<int> Pick({probs[0], probs[1], probs[2]});
    return Pick(rng);
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
  if (!PIBT::USE_PIBT_BANDIT) return;
  PIBT_ARM_PULLS[arm] += 1;
  PIBT_ARM_REWARDS[arm] += reward;
  log_pibt_event(arm, reward);
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

void PIBT::set_reward_weights(double w_goal, double w_delay, double w_stay,
                              double w_leave, double w_occ,
                              double w_congestion, double w_no_progress)
{
  auto clamp_w = [](double x) { return std::max(0.0, std::min(3.0, x)); };
  W_GOAL = clamp_w(w_goal);
  W_DELAY = clamp_w(w_delay);
  W_STAY = clamp_w(w_stay);
  W_LEAVE = clamp_w(w_leave);
  W_OCC = clamp_w(w_occ);
  W_CONG = clamp_w(w_congestion);
  W_NOPROG = clamp_w(w_no_progress);
}

void PIBT::set_runtime_config(bool events_log_enabled, int regret_trials)
{
  EVENTS_LOG_ENABLED = events_log_enabled;
  REGRET_TRIALS = std::max(1, regret_trials);

  if (!EVENTS_LOG_ENABLED && PIBT_EVENTS_LOG.is_open()) {
    PIBT_EVENTS_LOG.close();
    PIBT_EVENTS_LOG_READY = false;
  }
}

PIBT::PIBT(const Instance *_ins, DistTable *_D, const Deadline *_deadline,
           int seed)
    : ins(_ins),
      MT(seed),
      rrd(0, 1),
      N(ins->N),
      V_size(ins->G.size()),
      D(_D),
      deadline(_deadline),
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
  if (is_expired(deadline)) return false;
  bool success = true;
  // setup cache & constraints check
  for (auto i = 0; i < N; ++i) {
    if (is_expired(deadline)) return false;
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
      if (is_expired(deadline)) return false;
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
  if (is_expired(deadline)) return false;
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
      if (is_expired(deadline)) return false;
      if (occupied_now[u->id] != NO_AGENT) {
        neighbor_agents[num_neighbor_agents] = occupied_now[u->id];
        ++num_neighbor_agents;
      }
    }
  }

  auto get_successor_cost = [&](Vertex *u, int mode, bool swap = false) {
    if (is_expired(deadline)) return std::make_tuple(INT_MAX / 4, INT_MAX / 4, 1.0f);
    auto e = rrd(MT);
    if (swap) return std::make_tuple(-D->get(i, u), 0, e);

    int hindrance = 0;
    if (HINDRANCE) {
      for (auto k = 0; k < num_neighbor_agents; ++k) {
        if (is_expired(deadline)) return std::make_tuple(INT_MAX / 4, INT_MAX / 4, 1.0f);
        auto &&j = neighbor_agents[k];
        if (Q_from[j] != u && D->get(j, u) < D->get(j, Q_from[j])) {
          hindrance += 1;
        }
      }
    }
    auto regret_from_neighbors = [&]() {
      int value = 0;
      for (auto nb : u->neighbors) {
        if (is_expired(deadline)) return INT_MAX / 8;
        const auto j = occupied_now[nb->id];
        if (j != NO_AGENT && j != i && D->get(j, u) < D->get(j, Q_from[j])) {
          value += 1;
        }
      }
      return value;
    };

    int regret = 0;
    if (mode == 2 && REGRET_TRIALS > 1) {
      std::vector<int> candidates;
      candidates.reserve(u->neighbors.size());
      for (auto nb : u->neighbors) {
        if (is_expired(deadline)) return std::make_tuple(INT_MAX / 4, INT_MAX / 4, 1.0f);
        const auto j = occupied_now[nb->id];
        if (j != NO_AGENT && j != i) candidates.push_back(j);
      }

      if (candidates.empty()) {
        regret = 0;
      } else {
        std::uniform_int_distribution<int> pick_idx(0, (int)candidates.size() - 1);
        const int sample_count = (int)candidates.size();
        double acc = 0.0;
        for (int t = 0; t < REGRET_TRIALS; ++t) {
          int trial_regret = 0;
          for (int s = 0; s < sample_count; ++s) {
            if (is_expired(deadline)) return std::make_tuple(INT_MAX / 4, INT_MAX / 4, 1.0f);
            const int j = candidates[pick_idx(MT)];
            if (D->get(j, u) < D->get(j, Q_from[j])) trial_regret += 1;
          }
          acc += trial_regret;
        }
        regret = (int)std::lround(acc / std::max(1, REGRET_TRIALS));
      }
    } else {
      regret = regret_from_neighbors();
      if (regret >= INT_MAX / 16) return std::make_tuple(INT_MAX / 4, INT_MAX / 4, 1.0f);
    }

    if (mode == 0) return std::make_tuple(D->get(i, u), hindrance, e);
    if (mode == 1) return std::make_tuple(D->get(i, u), hindrance + 1, e);
    return std::make_tuple(D->get(i, u), hindrance + regret + 1, e);
  };

  // set C_next
  for (size_t k = 0; k <= K; ++k) {
    if (is_expired(deadline)) return false;
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
    if (is_expired(deadline)) return false;
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
    int local_congestion = 0;
    for (auto nb : u->neighbors) {
      if (is_expired(deadline)) return false;
      const auto nb_agent = occupied_now[nb->id];
      if (nb_agent != NO_AGENT && nb_agent != i) local_congestion += 1;
    }
    const double congestion_penalty = 0.05 * static_cast<double>(local_congestion);
    const double no_progress_penalty = (!at_goal_now && d_next >= d_now) ? 0.08 : 0.0;
    const double goal_bonus = (!at_goal_now && at_goal_next) ? 0.50 : 0.0;
    ST_GOAL.update(goal_bonus);
    ST_DELAY.update(delay_penalty);
    ST_STAY.update(stay_penalty);
    ST_LEAVE.update(leave_goal_penalty);
    ST_OCC.update(occupancy_penalty);
    ST_CONG.update(congestion_penalty);
    ST_NOPROG.update(no_progress_penalty);
    const bool zscore = (REWARD_AUTOSCALE_MODE == "zscore");
    const double goal_term = zscore ? ST_GOAL.z(goal_bonus) : goal_bonus;
    const double delay_term = zscore ? ST_DELAY.z(delay_penalty) : delay_penalty;
    const double stay_term = zscore ? ST_STAY.z(stay_penalty) : stay_penalty;
    const double leave_term = zscore ? ST_LEAVE.z(leave_goal_penalty) : leave_goal_penalty;
    const double occ_term = zscore ? ST_OCC.z(occupancy_penalty) : occupancy_penalty;
    const double cong_term = zscore ? ST_CONG.z(congestion_penalty) : congestion_penalty;
    const double noprogress_term = zscore ? ST_NOPROG.z(no_progress_penalty) : no_progress_penalty;
    const double raw_reward =
        W_GOAL * goal_term - W_DELAY * delay_term - W_STAY * stay_term -
      W_LEAVE * leave_term - W_OCC * occ_term - W_CONG * cong_term -
      W_NOPROG * noprogress_term;
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
      W_CONG = std::max(0.0, std::min(3.0, W_CONG - lr * err * cong_term));
      W_NOPROG =
          std::max(0.0, std::min(3.0, W_NOPROG - lr * err * noprogress_term));
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
    if (is_expired(deadline)) return false;
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
    if (is_expired(deadline)) return false;
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
