#ifndef MCTS_H
#define MCTS_H
#include "board.h"
#include <algorithm>
#include <atomic>
#include <cmath>
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
  long long total_lookups = 0;
  long long total_hits = 0;
  long long total_fillicides = 0;
  // roots is declared last so it is destroyed first: nodes' destructors touch the
  // table and tree_lock, which must still be alive when they run.
  std::vector<std::shared_ptr<MCTSNode>> roots;
  MCTSTree();
  std::shared_ptr<MCTSNode> get_node(const Board &new_board, std::shared_ptr<MCTSNode> new_parent);
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
  void filicide();
  void expand();
  void backpropagate(const Board &board, const std::vector<std::shared_ptr<MCTSNode>>& path);
  grid_coord get_move() const;
  policy_vec get_policy() const;
  MCTSNode(const Board &board, std::shared_ptr<MCTSNode> parent, MCTSTree *host);
  ~MCTSNode();
};

#endif