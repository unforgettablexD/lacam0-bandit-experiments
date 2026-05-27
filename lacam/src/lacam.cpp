#include "../include/lacam.hpp"

bool LaCAM::ANYTIME = false;
float LaCAM::RANDOM_INSERT_PROB1 = 0.001;
float LaCAM::RANDOM_INSERT_PROB2 = 0.001;
bool LaCAM::USE_ORDER_BANDIT = true;
bool LaCAM::USE_BRANCH_BANDIT = true;
bool LaCAM::USE_SCHED_BANDIT = true;
bool LaCAM::USE_RANDOM_BANDIT = true;
bool LaCAM::USE_BANDIT_HIERARCHY = false;
std::string LaCAM::BANDIT_POLICY = "ucb1";
double LaCAM::BANDIT_EPSILON = 0.10;
double LaCAM::BANDIT_EPSILON_FINAL = 0.10;
int LaCAM::BANDIT_EPSILON_DECAY_STEPS = 0;
int LaCAM::PIBT_ROLLOUTS = 1;
int LaCAM::PIBT_ROLLOUTS_AFTER_GOAL = 1;
int LaCAM::PIBT_ROLLOUTS_EARLY_STOP_MARGIN = 0;
std::string LaCAM::ORDER_BANDIT_MODE = "coarse3";
std::string LaCAM::ORDER_AGENT_REWARD = "first_only";

namespace {
constexpr int BANDIT_ARM_COUNT = 3;
std::array<int, BANDIT_ARM_COUNT> ORDER_PULLS = {0, 0, 0};
std::array<double, BANDIT_ARM_COUNT> ORDER_REWARDS = {0.0, 0.0, 0.0};
std::array<int, BANDIT_ARM_COUNT> BRANCH_PULLS = {0, 0, 0};
std::array<double, BANDIT_ARM_COUNT> BRANCH_REWARDS = {0.0, 0.0, 0.0};
std::array<int, BANDIT_ARM_COUNT> SCHED_PULLS = {0, 0, 0};
std::array<double, BANDIT_ARM_COUNT> SCHED_REWARDS = {0.0, 0.0, 0.0};
std::array<int, BANDIT_ARM_COUNT> RAND_PULLS = {0, 0, 0};
std::array<double, BANDIT_ARM_COUNT> RAND_REWARDS = {0.0, 0.0, 0.0};
std::array<int, BANDIT_ARM_COUNT> PIBT_PULLS = {0, 0, 0};
std::array<double, BANDIT_ARM_COUNT> PIBT_REWARDS = {0.0, 0.0, 0.0};
std::array<std::array<int, BANDIT_ARM_COUNT>, BANDIT_ARM_COUNT> ORDER_PULLS_C = {};
std::array<std::array<double, BANDIT_ARM_COUNT>, BANDIT_ARM_COUNT> ORDER_REWARDS_C = {};
std::array<std::array<std::array<int, BANDIT_ARM_COUNT>, BANDIT_ARM_COUNT>, BANDIT_ARM_COUNT> BRANCH_PULLS_C = {};
std::array<std::array<std::array<double, BANDIT_ARM_COUNT>, BANDIT_ARM_COUNT>, BANDIT_ARM_COUNT> BRANCH_REWARDS_C = {};

double squash_reward(const double raw) { return std::tanh(raw); }

int pick_arm(std::array<int, BANDIT_ARM_COUNT> &pulls,
             std::array<double, BANDIT_ARM_COUNT> &rewards, bool enabled,
             std::mt19937 &rng)
{
  if (!enabled) return 0;
  // Keep arm-0 as safe default at startup; forced exploration of arm-1/2 can
  // destabilize search before any useful reward signal is observed.
  if (pulls[0] == 0) return 0;
  const auto policy = LaCAM::BANDIT_POLICY;
  if (policy == "epsilon_greedy" || policy == "eps" || policy == "epsilon") {
    int total = 0;
    for (const auto p : pulls) total += p;
    double eps = LaCAM::BANDIT_EPSILON;
    if (LaCAM::BANDIT_EPSILON_DECAY_STEPS > 0) {
      const double t = std::min(1.0, static_cast<double>(total) /
                                         static_cast<double>(LaCAM::BANDIT_EPSILON_DECAY_STEPS));
      eps = LaCAM::BANDIT_EPSILON +
            (LaCAM::BANDIT_EPSILON_FINAL - LaCAM::BANDIT_EPSILON) * t;
    }
    std::uniform_real_distribution<double> U01(0.0, 1.0);
    std::uniform_int_distribution<int> UArm(0, BANDIT_ARM_COUNT - 1);
    if (U01(rng) < eps) return UArm(rng);
    int best = 0;
    double best_mean = -1e18;
    for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
      const auto mean = rewards[a] / std::max(1, pulls[a]);
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
      const auto denom = std::max(1, pulls[a]);
      const auto mean = rewards[a] / denom;
      const auto sigma = 1.0 / std::sqrt((double)denom);
      std::normal_distribution<double> N(mean, sigma);
      const auto s = N(rng);
      if (s > best_sample) {
        best_sample = s;
        best = a;
      }
    }
    return best;
  }
  if (policy == "softmax" || policy == "boltzmann") {
    const double temp = std::max(0.02, LaCAM::BANDIT_EPSILON);
    std::array<double, BANDIT_ARM_COUNT> logits = {0.0, 0.0, 0.0};
    double max_logit = -1e18;
    for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
      const auto mean = rewards[a] / std::max(1, pulls[a]);
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
  int total = 0;
  for (const auto p : pulls) total += p;
  int best = 0;
  double best_score = -1e18;
  for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
    const auto denom = std::max(1, pulls[a]);
    const auto mean = rewards[a] / denom;
    const auto bonus = std::sqrt(2.0 * std::log((double)std::max(1, total)) /
                                 (double)denom);
    const auto score = mean + bonus;
    if (score > best_score) {
      best_score = score;
      best = a;
    }
  }
  return best;
}

void update_arm(std::array<int, BANDIT_ARM_COUNT> &pulls,
                std::array<double, BANDIT_ARM_COUNT> &rewards, const int arm,
                const double reward)
{
  pulls[arm] += 1;
  rewards[arm] += reward;
}
}  // namespace

void LaCAM::set_bandit_config(bool use_order_bandit, bool use_branch_bandit,
                              bool use_sched_bandit, bool use_random_bandit,
                              bool use_bandit_hierarchy,
                              const std::string &bandit_policy,
                              double bandit_epsilon,
                              double bandit_epsilon_final,
                              int bandit_epsilon_decay_steps,
                              const std::string &order_bandit_mode,
                              const std::string &order_agent_reward)
{
  USE_ORDER_BANDIT = use_order_bandit;
  USE_BRANCH_BANDIT = use_branch_bandit;
  USE_SCHED_BANDIT = use_sched_bandit;
  USE_RANDOM_BANDIT = use_random_bandit;
  USE_BANDIT_HIERARCHY = use_bandit_hierarchy;
  BANDIT_POLICY = bandit_policy.empty() ? "ucb1" : bandit_policy;
  BANDIT_EPSILON = std::max(0.0, std::min(1.0, bandit_epsilon));
  BANDIT_EPSILON_FINAL = std::max(0.0, std::min(1.0, bandit_epsilon_final));
  BANDIT_EPSILON_DECAY_STEPS = std::max(0, bandit_epsilon_decay_steps);
  ORDER_BANDIT_MODE = order_bandit_mode.empty() ? "coarse3" : order_bandit_mode;
  ORDER_AGENT_REWARD = order_agent_reward.empty() ? "first_only" : order_agent_reward;
}

bool CompareHNodePointers::operator()(const HNode *l, const HNode *r) const
{
  const auto N = l->Q.size();
  for (size_t i = 0; i < N; ++i) {
    if (l->Q[i] != r->Q[i]) return l->Q[i]->id < r->Q[i]->id;
  }
  return false;
}

HNode::HNode(Config _Q, DistTable *D, HNode *_parent, int _g, int _h)
    : Q(std::move(_Q)),
      parent(_parent),
      neighbors(),
      g(_g),
      h(_h),
      f(g + h),
      depth(parent == nullptr ? 0 : parent->depth + 1),
      priorities(Q.size()),
      order(Q.size(), 0),
      search_tree()
{
  if (parent != nullptr) parent->neighbors.insert(this);
  search_tree.push(new LNode());
  const int N = Q.size();
  for (int i = 0; i < N; ++i) {
    // set priorities
    if (parent == nullptr) {
      // initialize
      priorities[i] = (float)D->get(i, Q[i]) / 10000;
    } else {
      // dynamic priorities, akin to PIBT
      if (D->get(i, Q[i]) != 0) {
        priorities[i] = parent->priorities[i] + 1;
      } else {
        priorities[i] = parent->priorities[i] - (int)parent->priorities[i];
      }
    }
  }

  // set order
  auto cmp = [&](int i, int j) { return priorities[i] > priorities[j]; };
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), cmp);
}

HNode::~HNode()
{
  while (!search_tree.empty()) {
    delete search_tree.front();
    search_tree.pop();
  }
}

LNode::LNode() : who(), where(), depth(0) {}

LNode::LNode(LNode *parent, int i, Vertex *v)
    : who(parent->who), where(parent->where), depth(parent->depth + 1)
{
  who.push_back(i);
  where.push_back(v);
}

LNode::~LNode(){};

LaCAM::LaCAM(const Instance *_ins, DistTable *_D, int _verbose,
             const Deadline *_deadline, int _seed)
    : ins(_ins),
      D(_D),
      deadline(_deadline),
      seed(_seed),
      MT(seed),
      rrd(0, 1),
      verbose(_verbose),
      order_agent_pulls(ins->N, 0),
      order_agent_rewards(ins->N, 0.0),
      pibt(ins, D, deadline, seed),
      H_goal(nullptr),
      OPEN(),
      loop_cnt(0)
{
}

LaCAM::~LaCAM() {}

Solution LaCAM::solve()
{
  solver_info(1, "LaCAM begins");

  // setup search
  auto EXPLORED = std::unordered_map<Config, HNode *, ConfigHasher>();
  HNodes GC_HNodes;

  // insert initial node
  auto H_init = new HNode(ins->starts, D);
  OPEN.push_front(H_init);
  EXPLORED[H_init->Q] = H_init;
  GC_HNodes.push_back(H_init);
  int best_h_seen = H_init->h;
  int no_improve_iters = 0;
  int restart_cooldown = 0;
  constexpr int STALL_TRIGGER_ITERS = 600;
  constexpr int STALL_RESTART_PERIOD = 120;

  // search loop
  solver_info(2, "search iteration begins");
  while (!OPEN.empty() && !is_expired(deadline)) {
    ++loop_cnt;
    int sched_arm = pick_arm(SCHED_PULLS, SCHED_REWARDS, USE_SCHED_BANDIT, MT);
    // Safety: before the first feasible goal node is found, keep original
    // extraction behavior. Exploring scheduler alternatives too early can stall
    // search on hard instances.
    if (H_goal == nullptr) sched_arm = 0;
    const bool use_hierarchy_with_pibt =
      USE_BANDIT_HIERARCHY && PIBT::USE_PIBT_BANDIT;
    const int pibt_arm = use_hierarchy_with_pibt
                 ? pick_arm(PIBT_PULLS, PIBT_REWARDS, true, MT)
                 : -1;
    // Default must preserve original LaCAM behavior: take OPEN.front() (LIFO/DFS).
    int hidx = 0;
    if (sched_arm == 1) {
      int best_idx = hidx;
      int best_score = INT_MAX;
      for (size_t oi = 0; oi < OPEN.size(); ++oi) {
        auto *cand = OPEN[oi];
        if (cand->h < best_score) {
          best_score = cand->h;
          best_idx = (int)oi;
        }
      }
      hidx = best_idx;
    } else if (sched_arm == 2 && OPEN.size() > 1) {
      const float p_best = 0.5f;
      if (rrd(MT) < p_best) {
        int best_idx = hidx;
        int best_score = INT_MAX;
        for (size_t oi = 0; oi < OPEN.size(); ++oi) {
          auto *cand = OPEN[oi];
          if (cand->h < best_score) {
            best_score = cand->h;
            best_idx = (int)oi;
          }
        }
        hidx = best_idx;
      }
    }

    // random insert
    if (H_goal != nullptr) {
      auto r = rrd(MT);
      if (r < RANDOM_INSERT_PROB2 / 2) {
        OPEN.push_front(H_init);
      } else if (r < RANDOM_INSERT_PROB2) {
        auto H = OPEN[get_random_int(MT, 0, OPEN.size() - 1)];
        OPEN.push_front(H);
      }
    }

    // do not pop here!
    auto H = OPEN[hidx];  // high-level node
    if (H->h < best_h_seen) {
      best_h_seen = H->h;
      no_improve_iters = 0;
      restart_cooldown = 0;
    } else {
      no_improve_iters += 1;
      if (restart_cooldown > 0) restart_cooldown -= 1;
    }
    const bool stall_mode = (no_improve_iters >= STALL_TRIGGER_ITERS);
    if (stall_mode && H_goal == nullptr && restart_cooldown == 0) {
      OPEN.push_front(H_init);
      restart_cooldown = STALL_RESTART_PERIOD;
    }

    // check upper bounds
    if (H_goal != nullptr && H->f >= H_goal->g) {
      OPEN.erase(OPEN.begin() + hidx);
      if (USE_SCHED_BANDIT)
        update_arm(SCHED_PULLS, SCHED_REWARDS, sched_arm,
                   squash_reward(-0.3 + (stall_mode && sched_arm != 0 ? -0.15 : 0.0)));
      solver_info(5, "prune, g=", H->g, " >= ", H_goal->g);
      OPEN.push_front(H_init);
      continue;
    }

    // check goal condition
    if (H_goal == nullptr && is_same_config(H->Q, ins->goals)) {
      H_goal = H;
      solver_info(2, "found solution, g=", H->g, ", depth=", H->depth);
      if (!ANYTIME) break;
      continue;
    }

    // extract constraints
    if (H->search_tree.empty()) {
      OPEN.erase(OPEN.begin() + hidx);
      if (USE_SCHED_BANDIT)
        update_arm(SCHED_PULLS, SCHED_REWARDS, sched_arm,
                   squash_reward(-0.2 + (stall_mode && sched_arm != 0 ? -0.10 : 0.0)));
      continue;
    }
    auto L = H->search_tree.front();
    H->search_tree.pop();

    // low level search
    if (L->depth < H->Q.size()) {
      std::vector<int> order = H->order;
      int order_arm = 0;
      if (ORDER_BANDIT_MODE == "agent_level") {
        std::vector<std::pair<double, int>> sampled;
        sampled.reserve(order.size());
        for (auto aid : order) {
          double mean = 0.0;
          double sigma = 1.0;
          if (order_agent_pulls[aid] > 0) {
            mean = order_agent_rewards[aid] / order_agent_pulls[aid];
            sigma = 1.0 / std::sqrt((double)order_agent_pulls[aid]);
          }
          std::normal_distribution<double> N(mean, sigma);
          sampled.push_back({N(MT), aid});
        }
        std::sort(sampled.begin(), sampled.end(),
                  [](const auto &a, const auto &b) { return a.first > b.first; });
        order.clear();
        for (const auto &p : sampled) order.push_back(p.second);
      } else {
        if (use_hierarchy_with_pibt) {
          order_arm = pick_arm(ORDER_PULLS_C[pibt_arm], ORDER_REWARDS_C[pibt_arm], USE_ORDER_BANDIT, MT);
        } else {
          order_arm = pick_arm(ORDER_PULLS, ORDER_REWARDS, USE_ORDER_BANDIT, MT);
        }
        if (order_arm == 1) {
          std::reverse(order.begin(), order.end());
        } else if (order_arm == 2) {
          std::shuffle(order.begin(), order.end(), MT);
        }
      }

      const auto i = order[L->depth];
      auto C = H->Q[i]->actions;

      int rand_arm = pick_arm(RAND_PULLS, RAND_REWARDS, USE_RANDOM_BANDIT, MT);
      double p_shuffle = 0.1;
      if (rand_arm == 1) p_shuffle = 0.5;
      if (rand_arm == 2) p_shuffle = 0.9;
      if (stall_mode) p_shuffle = std::min(p_shuffle, 0.2);
      if (rrd(MT) < p_shuffle) std::shuffle(C.begin(), C.end(), MT);

      int branch_arm = 0;
      if (use_hierarchy_with_pibt) {
        branch_arm = pick_arm(BRANCH_PULLS_C[pibt_arm][order_arm], BRANCH_REWARDS_C[pibt_arm][order_arm], USE_BRANCH_BANDIT, MT);
      } else {
        branch_arm = pick_arm(BRANCH_PULLS, BRANCH_REWARDS, USE_BRANCH_BANDIT, MT);
      }
      if (branch_arm == 1) {
        std::sort(C.begin(), C.end(),
                  [&](const Vertex *a, const Vertex *b) { return D->get(i, a) < D->get(i, b); });
      } else if (branch_arm == 2) {
        std::sort(C.begin(), C.end(), [&](const Vertex *a, const Vertex *b) {
          int oa = 0, ob = 0;
          for (auto *v : H->Q) {
            if (v->id == a->id) ++oa;
            if (v->id == b->id) ++ob;
          }
          if (oa != ob) return oa < ob;
          return D->get(i, a) < D->get(i, b);
        });
      }

      double order_reward = 0.0;
      const int first = order.empty() ? -1 : order.front();
      if (first >= 0) {
        const auto d0 = D->get(first, H->Q[first]);
        order_reward = (d0 > 0) ? 0.2 : 0.0;
        if (order_arm == 0) order_reward += 0.1;
      }
      if (stall_mode && order_arm != 0) order_reward -= 0.05;
      if (ORDER_BANDIT_MODE == "agent_level") {
        if (!order.empty()) {
          if (ORDER_AGENT_REWARD == "topk") {
            const int k = std::min<int>(5, order.size());
            for (int t = 0; t < k; ++t) {
              const int aid = order[t];
              order_agent_pulls[aid] += 1;
              order_agent_rewards[aid] += order_reward / std::max(1, k);
            }
          } else {
            const int aid = order.front();
            order_agent_pulls[aid] += 1;
            order_agent_rewards[aid] += order_reward;
          }
        }
      } else if (use_hierarchy_with_pibt) {
        update_arm(ORDER_PULLS_C[pibt_arm], ORDER_REWARDS_C[pibt_arm], order_arm, squash_reward(order_reward));
      } else if (USE_ORDER_BANDIT) {
        update_arm(ORDER_PULLS, ORDER_REWARDS, order_arm, squash_reward(order_reward));
      }

      if (use_hierarchy_with_pibt) {
        double branch_reward = (branch_arm == 1 ? 0.15 : (branch_arm == 2 ? 0.1 : 0.05));
        if (stall_mode && branch_arm != 1) branch_reward -= 0.08;
        update_arm(BRANCH_PULLS_C[pibt_arm][order_arm], BRANCH_REWARDS_C[pibt_arm][order_arm], branch_arm,
                   squash_reward(branch_reward));
      } else if (USE_BRANCH_BANDIT) {
        double branch_reward = (branch_arm == 1 ? 0.15 : (branch_arm == 2 ? 0.1 : 0.05));
        if (stall_mode && branch_arm != 1) branch_reward -= 0.08;
        update_arm(BRANCH_PULLS, BRANCH_REWARDS, branch_arm,
                   squash_reward(branch_reward));
      }
      if (USE_RANDOM_BANDIT) {
        double random_reward = 0.1 + 0.1 * (1.0 - p_shuffle);
        if (stall_mode && rand_arm != 0) random_reward -= 0.10;
        update_arm(RAND_PULLS, RAND_REWARDS, rand_arm,
                   squash_reward(random_reward));
      }

      for (auto u : C) H->search_tree.push(new LNode(L, i, u));
    }

    // create successors at the high-level search
    auto Q_to = Config(ins->N, nullptr);
    auto rollout_budget = 1;
    if (PIBT_ROLLOUTS > 1 && stall_mode) {
      rollout_budget = PIBT_ROLLOUTS;
    } else if (PIBT_ROLLOUTS_AFTER_GOAL > 1 && H_goal != nullptr) {
      rollout_budget = std::min(PIBT_ROLLOUTS, PIBT_ROLLOUTS_AFTER_GOAL);
    }
    if (use_hierarchy_with_pibt) PIBT::set_forced_pibt_arm(pibt_arm);
    auto res = set_new_config(H, L, Q_to, rollout_budget);
    if (use_hierarchy_with_pibt) PIBT::set_forced_pibt_arm(-1);
    delete L;
    if (!res) {
      if (use_hierarchy_with_pibt)
        update_arm(PIBT_PULLS, PIBT_REWARDS, pibt_arm, squash_reward(-0.8));
      continue;
    }
    if (use_hierarchy_with_pibt)
      update_arm(PIBT_PULLS, PIBT_REWARDS, pibt_arm, squash_reward(0.4));

    // check explored list
    auto iter = EXPLORED.find(Q_to);
    if (iter == EXPLORED.end()) {
      // new one -> insert
      const auto g_val = get_g_val(H, Q_to);
      const auto h_val = get_h_val(Q_to);
      auto H_new = new HNode(std::move(Q_to), D, H, g_val, h_val);
      OPEN.push_front(H_new);
      EXPLORED[H_new->Q] = H_new;
      GC_HNodes.push_back(H_new);
      if (USE_SCHED_BANDIT)
        update_arm(SCHED_PULLS, SCHED_REWARDS, sched_arm,
                   squash_reward(0.4 + (stall_mode && sched_arm == 0 ? 0.05 : 0.0)));
    } else {
      // known configuration
      auto H_known = iter->second;
      rewrite(H, H_known);

      if (rrd(MT) >= RANDOM_INSERT_PROB1) {
        OPEN.push_front(iter->second);  // usual
      } else {
        solver_info(3, "random restart");
        OPEN.push_front(H_init);  // sometimes
      }
      if (USE_SCHED_BANDIT)
        update_arm(SCHED_PULLS, SCHED_REWARDS, sched_arm,
                   squash_reward(-0.1 + (stall_mode && sched_arm != 0 ? -0.10 : 0.0)));
    }
  }

  // backtrack
  Solution solution;
  {
    auto H = H_goal;
    while (H != nullptr) {
      solution.push_back(H->Q);
      H = H->parent;
    }
    std::reverse(solution.begin(), solution.end());
  }

  // solution
  if (solution.empty()) {
    if (OPEN.empty()) {
      solver_info(2, "fin. unsolvable instance");
    } else {
      solver_info(2, "fin. reach time limit");
    }
  } else {
    if (OPEN.empty()) {
      solver_info(2, "fin. optimal solution, g=", H_goal->g,
                  ", depth=", H_goal->depth);
    } else {
      solver_info(2, "fin. suboptimal solution, g=", H_goal->g,
                  ", depth=", H_goal->depth);
    }
  }

  // end processing
  for (auto &&H : GC_HNodes) delete H;  // memory management

  return solution;
}

bool LaCAM::set_new_config(HNode *H, LNode *L, Config &Q_to, int rollout_budget)
{
  auto Q_base = Config(ins->N, nullptr);
  for (uint d = 0; d < L->depth; ++d) Q_base[L->who[d]] = L->where[d];

  const int rollout_count = std::max(1, rollout_budget);
  auto best_f = INT_MAX;
  auto first_f = INT_MAX;
  auto found = false;

  for (int k = 0; k < rollout_count; ++k) {
    auto Q_cand = Q_base;
    const auto ok = pibt.set_new_config(H->Q, Q_cand, H->order);
    if (!ok) continue;

    const auto cand_f = get_edge_cost(H->Q, Q_cand) + get_h_val(Q_cand);
    if (!found) first_f = cand_f;
    if (!found || cand_f < best_f) {
      best_f = cand_f;
      Q_to = std::move(Q_cand);
      found = true;
    }

    if (found && k > 0 && PIBT_ROLLOUTS_EARLY_STOP_MARGIN > 0 &&
        first_f < INT_MAX &&
        best_f <= first_f - PIBT_ROLLOUTS_EARLY_STOP_MARGIN) {
      break;
    }
  }

  return found;
}

void LaCAM::rewrite(HNode *H_from, HNode *H_to)
{
  if (!ANYTIME) return;

  // update neighbors
  H_from->neighbors.insert(H_to);

  // Dijkstra
  std::queue<HNode *> Q({H_from});  // queue is sufficient
  while (!Q.empty()) {
    auto n_from = Q.front();
    Q.pop();
    for (auto n_to : n_from->neighbors) {
      auto g_val = n_from->g + get_edge_cost(n_from->Q, n_to->Q);
      if (g_val < n_to->g) {
        if (n_to == H_goal) {
          solver_info(2, "cost update: g=", H_goal->g, " -> ", g_val,
                      ", depth=", H_goal->depth, " -> ", n_from->depth + 1);
        }
        n_to->g = g_val;
        n_to->f = n_to->g + n_to->h;
        n_to->parent = n_from;
        n_to->depth = n_from->depth + 1;
        Q.push(n_to);
        if (H_goal != nullptr && n_to->f < H_goal->f) {
          OPEN.push_front(n_to);
          solver_info(4, "reinsert: g=", n_to->g, " < ", H_goal->g);
        }
      }
    }
  }
}

int LaCAM::get_g_val(HNode *H_parent, const Config &Q_to)
{
  return H_parent->g + get_edge_cost(H_parent->Q, Q_to);
}

int LaCAM::get_h_val(const Config &Q)
{
  auto c = 0;
  for (size_t i = 0; i < ins->N; ++i) c += D->get(i, Q[i]);
  return c;
}

int LaCAM::get_edge_cost(const Config &Q1, const Config &Q2)
{
  auto cost = 0;
  for (size_t i = 0; i < ins->N; ++i) {
    if (Q1[i] != ins->goals[i] || Q2[i] != ins->goals[i]) {
      cost += 1;
    }
  }
  return cost;
}
