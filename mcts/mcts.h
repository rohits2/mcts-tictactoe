#ifndef MCTS_H
#define MCTS_H
#include "board.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

typedef struct _float_grid_wrapper {
  float policy[9][9];
} policy_vec;

// Slack, in win-probability units, applied to every alpha-beta cutoff during
// pruning. A child branch is only discarded once it is worse than the line we
// keep by more than this much, so moves within PRUNE_MARGIN of optimal survive
// instead of collapsing to a single principal variation. 0 == exact alpha-beta.
static constexpr float PRUNE_MARGIN = 0.05f;

class MCTSNode;

// A fixed set of worker threads created once and reused for every search batch
// and prune, so we no longer pay thread-creation cost on each call. parallel_for
// hands out the `count` task indices dynamically (whoever is free grabs the
// next), so uneven work -- e.g. one huge principal-variation subtree during a
// prune -- self-balances, and the calling thread pitches in rather than idling.
class ThreadPool {
public:
  explicit ThreadPool(unsigned num_threads);
  ~ThreadPool();
  unsigned size() const { return (unsigned)threads_.size(); }
  void parallel_for(int count, const std::function<void(int)> &fn);

private:
  void worker_loop();
  std::vector<std::thread> threads_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> tasks_;
  bool stop_ = false;
};

// Transposition-table value. The table is keyed by a 64-bit board hash so the
// ~48-byte Board lives once (inside the node) instead of being duplicated as the
// map key. `wp` is locked on lookup -- with operator== verifying the board to
// reject the (astronomically rare) hash collision -- and `self` is the raw
// identity used to erase exactly this node's entry from its destructor, where
// `wp` has already expired.
struct NodeRef {
  std::weak_ptr<MCTSNode> wp;
  MCTSNode *self;
};

class MCTSTree {
public:
  std::recursive_mutex tree_lock;
  std::unordered_multimap<uint64_t, NodeRef> transposition_table;
  // Atomic so the UI thread can read them lock-free for the stats display while
  // the search thread increments them.
  std::atomic<long long> total_lookups{0};
  std::atomic<long long> total_hits{0};
  std::atomic<long long> total_fillicides{0};
  std::atomic<long long> total_iterations{0}; // cumulative MCTS rollouts (for rollouts/sec)
  // Persistent search threads, reused by parallel_mcts() and prune_alpha_beta().
  // Declared before roots so it is destroyed after them -- the pool threads are
  // idle outside a parallel_for, so the node-cascade in roots' destructor runs
  // with the table/tree_lock still alive, then the pool joins.
  ThreadPool pool_;
  // roots is declared last so it is destroyed first: nodes' destructors touch the
  // table and tree_lock, which must still be alive when they run.
  std::vector<std::shared_ptr<MCTSNode>> roots;
  MCTSTree();
  std::shared_ptr<MCTSNode> get_node(const Board &new_board, std::shared_ptr<MCTSNode> new_parent);
  std::shared_ptr<MCTSNode> find_node(const Board &new_board); // read-only lookup; never inserts/roots
  float transposition_hitrate();
  int transposition_size();
  long long purges();
  void mcts(const Board &board, int num_iterations);
  void mcts(std::shared_ptr<MCTSNode> node, int num_iterations);
  void parallel_mcts(const Board &board, int num_iterations);
  void parallel_mcts(std::shared_ptr<MCTSNode> node, int num_iterations);
  void prune_soft(const Board& board);
  void prune_soft(std::shared_ptr<MCTSNode> node);
  void prune_feather(const Board& board);
  void prune_feather(std::shared_ptr<MCTSNode> node);
  void prune_alpha_beta(const Board& board, float margin = PRUNE_MARGIN);
  void prune_alpha_beta(std::shared_ptr<MCTSNode> node, float margin = PRUNE_MARGIN);
  // Proven game-theoretic winner of the position (PLAYER_X / PLAYER_O /
  // PLAYER_TIE), or PLAYER_NONE if not yet conclusively proven.
  char proven_winner(std::shared_ptr<MCTSNode> node);


protected:
};

class MCTSNode : public std::enable_shared_from_this<MCTSNode> {
public:
  Board board;
  MCTSTree *tree;
  std::vector<std::weak_ptr<MCTSNode>> parents;
  std::vector<std::shared_ptr<MCTSNode>> children;
  // Atomic because the search (worker) thread mutates these while the UI thread
  // reads them (get_value, win/tie probabilities), and select() bumps the leaf's
  // visit count without holding the node lock.
  std::atomic<unsigned> visits{0};
  std::atomic<unsigned> wins{0};
  std::atomic<unsigned> ties{0};
  std::atomic<bool> expanded{false};
  // Set under `lock` by the one thread that claims expansion, so that under
  // parallel search the other threads that reach this leaf skip it instead of
  // racing to build the same children. `expanded` only flips true once the
  // children are installed, so a node is never observed expanded-but-empty.
  bool expanding = false;
  mutable std::recursive_mutex lock;
  float Q();
  float parent_Q();
  float U();
  float PUCT();
  int ref_count = 0;
  std::shared_ptr<MCTSNode> max_PUCT();
  std::vector<std::shared_ptr<MCTSNode>> select();
  void prune_ancestors() ;
  void prune_ancestors(std::shared_ptr<const MCTSNode> node_to_keep) ;
  float minimax_value(std::unordered_map<const MCTSNode *, float> &memo);
  void alpha_beta_prune(float alpha, float beta, float margin, std::unordered_map<const MCTSNode *, float> &memo,
                        std::unordered_set<const MCTSNode *> &done);
  char proven_winner(std::unordered_map<const MCTSNode *, char> &memo);
  void filicide();
  void expand();
  void backpropagate(const Board &board, const std::vector<std::shared_ptr<MCTSNode>>& path);
  grid_coord get_move() const;
  policy_vec get_policy() const;
  MCTSNode(const Board &board, std::shared_ptr<MCTSNode> parent, MCTSTree *host);
  ~MCTSNode();
};

#endif