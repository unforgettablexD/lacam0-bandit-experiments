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
  for (int a = 0; a < BANDIT_ARM_COUNT; ++a) {
    if (pulls[a] == 0) return a;
  }
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
      const auto mean = rewards[a] / pulls[a];
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
      const auto mean = rewards[a] / pulls[a];
      const auto sigma = 1.0 / std::sqrt((double)pulls[a]);
      std::normal_distribution<double> N(mean, sigma);
      const auto s = N(rng);
      if (s > best_sample) {
        best_sample = s;
        best = a;
      }
    }
    return best;
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
    const auto mean = rewards[a] / pulls[a];
    const auto bonus = std::sqrt(2.0 * std::log((double)total) / pulls[a]);
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
                              int bandit_epsilon_decay_steps)
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
      pibt(ins, D, seed),
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

  // search loop
  solver_info(2, "search iteration begins");
  while (!OPEN.empty() && !is_expired(deadline)) {
    ++loop_cnt;

    const int sched_arm = pick_arm(SCHED_PULLS, SCHED_REWARDS, USE_SCHED_BANDIT, MT);
    const int pibt_arm = USE_BANDIT_HIERARCHY ? pick_arm(PIBT_PULLS, PIBT_REWARDS, true, MT) : -1;
    int hidx = (int)OPEN.size() - 1;  // default: DFS/LIFO
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

    // check upper bounds
    if (H_goal != nullptr && H->f >= H_goal->g) {
      OPEN.erase(OPEN.begin() + hidx);
      update_arm(SCHED_PULLS, SCHED_REWARDS, sched_arm, squash_reward(-0.3));
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
      update_arm(SCHED_PULLS, SCHED_REWARDS, sched_arm, squash_reward(-0.2));
      continue;
    }
    auto L = H->search_tree.front();
    H->search_tree.pop();

    // low level search
    if (L->depth < H->Q.size()) {
      int order_arm = 0;
      if (USE_BANDIT_HIERARCHY) {
        order_arm = pick_arm(ORDER_PULLS_C[pibt_arm], ORDER_REWARDS_C[pibt_arm], USE_ORDER_BANDIT, MT);
      } else {
        order_arm = pick_arm(ORDER_PULLS, ORDER_REWARDS, USE_ORDER_BANDIT, MT);
      }
      std::vector<int> order = H->order;
      if (order_arm == 1) {
        std::reverse(order.begin(), order.end());
      } else if (order_arm == 2) {
        std::shuffle(order.begin(), order.end(), MT);
      }

      const auto i = order[L->depth];
      auto C = H->Q[i]->actions;

      int rand_arm = pick_arm(RAND_PULLS, RAND_REWARDS, USE_RANDOM_BANDIT, MT);
      double p_shuffle = 0.1;
      if (rand_arm == 1) p_shuffle = 0.5;
      if (rand_arm == 2) p_shuffle = 0.9;
      if (rrd(MT) < p_shuffle) std::shuffle(C.begin(), C.end(), MT);

      int branch_arm = 0;
      if (USE_BANDIT_HIERARCHY) {
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
      if (USE_BANDIT_HIERARCHY) {
        update_arm(ORDER_PULLS_C[pibt_arm], ORDER_REWARDS_C[pibt_arm], order_arm, squash_reward(order_reward));
      } else {
        update_arm(ORDER_PULLS, ORDER_REWARDS, order_arm, squash_reward(order_reward));
      }

      if (USE_BANDIT_HIERARCHY) {
        update_arm(BRANCH_PULLS_C[pibt_arm][order_arm], BRANCH_REWARDS_C[pibt_arm][order_arm], branch_arm,
                   squash_reward(branch_arm == 1 ? 0.15 : (branch_arm == 2 ? 0.1 : 0.05)));
      } else {
        update_arm(BRANCH_PULLS, BRANCH_REWARDS, branch_arm,
                   squash_reward(branch_arm == 1 ? 0.15 : (branch_arm == 2 ? 0.1 : 0.05)));
      }
      update_arm(RAND_PULLS, RAND_REWARDS, rand_arm,
                 squash_reward(0.1 + 0.1 * (1.0 - p_shuffle)));

      for (auto u : C) H->search_tree.push(new LNode(L, i, u));
    }

    // create successors at the high-level search
    auto Q_to = Config(ins->N, nullptr);
    if (USE_BANDIT_HIERARCHY) PIBT::set_forced_pibt_arm(pibt_arm);
    auto res = set_new_config(H, L, Q_to);
    if (USE_BANDIT_HIERARCHY) PIBT::set_forced_pibt_arm(-1);
    delete L;
    if (!res) {
      if (USE_BANDIT_HIERARCHY) update_arm(PIBT_PULLS, PIBT_REWARDS, pibt_arm, squash_reward(-0.8));
      continue;
    }
    if (USE_BANDIT_HIERARCHY) update_arm(PIBT_PULLS, PIBT_REWARDS, pibt_arm, squash_reward(0.4));

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
      update_arm(SCHED_PULLS, SCHED_REWARDS, sched_arm, squash_reward(0.4));
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
      update_arm(SCHED_PULLS, SCHED_REWARDS, sched_arm, squash_reward(-0.1));
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

bool LaCAM::set_new_config(HNode *H, LNode *L, Config &Q_to)
{
  for (uint d = 0; d < L->depth; ++d) Q_to[L->who[d]] = L->where[d];
  return pibt.set_new_config(H->Q, Q_to, H->order);
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
