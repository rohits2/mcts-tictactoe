#ifndef MCTS_H
#define MCTS_H
#include "board.h"
#include <algorithm>
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
#include <vector>

typedef struct _float_grid_wrapper {
  float policy[9][9];
} policy_vec;

class MCTSNode;

class MCTSTree {
public:
  std::vector<std::shared_ptr<MCTSNode>> roots;
  std::recursive_mutex tree_lock;
  std::unordered_map<Board, std::weak_ptr<MCTSNode>> transposition_table;
  long long total_lookups = 0;
  long long total_hits = 0;
  long long total_fillicides = 0;
  std::shared_ptr<MCTSNode> get_node(const Board &new_board, std::shared_ptr<MCTSNode> new_parent);
  float transposition_hitrate();
  int transposition_size();
  long long purges();
  void mcts(const Board &board, int num_iterations);
  void mcts(std::shared_ptr<MCTSNode> node, int num_iterations);
  void parallel_mcts(const Board &board, int num_iterations);
  void parallel_mcts(std::shared_ptr<MCTSNode> node, int num_iterations);
  void prune_hard(unsigned max_size);
  void prune_soft(const Board& board);
  void prune_soft(std::shared_ptr<MCTSNode> node);
  void prune_feather(const Board& board);
  void prune_feather(std::shared_ptr<MCTSNode> node);


protected:
};

class MCTSNode : public std::enable_shared_from_this<MCTSNode> {
public:
  Board board;
  MCTSTree *tree;
  std::vector<std::weak_ptr<MCTSNode>> parents;
  std::vector<std::shared_ptr<MCTSNode>> children;
  std::vector<grid_coord> moves;
  unsigned visits = 0;
  unsigned wins = 0;
  unsigned ties = 0;
  bool expanded = false;
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
  void prune_children(float reduction, bool recurse);
  void filicide();
  void expand();
  void backpropagate(const Board &board, const std::vector<std::shared_ptr<MCTSNode>>& path);
  grid_coord get_move() const;
  policy_vec get_policy() const;
  MCTSNode(const Board &board, std::shared_ptr<MCTSNode> parent, MCTSTree *host);
  ~MCTSNode();
};

#endif