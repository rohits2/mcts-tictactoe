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

using std::thread, std::unordered_map, std::find, std::shared_ptr, std::weak_ptr, std::pair, std::recursive_mutex,
    std::queue, std::uniform_int_distribution, std::min, std::make_shared, std::enable_shared_from_this, std::sqrt, std::find;

typedef struct _float_grid_wrapper {
    float policy[9][9];
} policy_vec;

class MCTSNode;

class MCTSTree {
  public:
    vector<shared_ptr<MCTSNode>> roots;
    recursive_mutex tree_lock;
    unordered_map<Board, weak_ptr<MCTSNode>> transposition_table;
    long long total_lookups = 0;
    long long total_hits = 0;
    long long total_fillicides = 0;
    shared_ptr<MCTSNode> get_node(const Board &new_board, shared_ptr<MCTSNode> new_parent);
    float transposition_hitrate();
    int transposition_size();
    long long purges();
    void mcts(const Board &board, int num_iterations);
    void parallel_mcts(const Board &board, int num_iterations);
    void prune(unsigned max_size);
};

class MCTSNode : public enable_shared_from_this<MCTSNode> {
  public:
    Board board;
    MCTSTree *tree;
    vector<weak_ptr<MCTSNode>> parents;
    vector<shared_ptr<MCTSNode>> children;
    vector<grid_coord> moves;
    unsigned visits = 0;
    unsigned wins = 0;
    unsigned ties = 0;
    bool expanded = false;
    mutable recursive_mutex lock;
    float Q();
    float parent_Q();
    float U();
    float PUCT();
    int ref_count = 0;
    shared_ptr<MCTSNode> max_PUCT();
    vector<shared_ptr<MCTSNode>> select();
    void prune_ancestors();
    void prune_ancestors(shared_ptr<MCTSNode> node_to_keep);
    void prune_children();
    void filicide();
    void expand();
    void backpropagate(const Board &board, vector<shared_ptr<MCTSNode>> path);
    grid_coord get_move() const;
    policy_vec get_policy() const;
    MCTSNode(const Board &board, shared_ptr<MCTSNode> parent, MCTSTree *host);
    ~MCTSNode();
};

#endif