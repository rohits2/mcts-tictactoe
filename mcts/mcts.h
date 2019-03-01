#ifndef MCTS_H
#define MCTS_H
#include "board.h"
#include <algorithm>
#include <limits>
#include <math.h>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <unordered_map>
#include <vector>

using std::thread, std::shared_mutex, std::unordered_map, std::find, std::shared_ptr, std::weak_ptr, std::pair, std::recursive_mutex, std::queue,
    std::uniform_int_distribution, std::min;

typedef struct _float_grid_wrapper {
    float policy[9][9];
} policy_vec;

class MCTSNode {
  public:
    static MCTSNode* get_node(const Board &new_board, MCTSNode *new_parent);
    ~MCTSNode();
    Board board;
    MCTSNode *parent;
    vector<MCTSNode *> children;
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
    MCTSNode *max_PUCT();
    MCTSNode *select();
    void prune_ancestors();
    void prune_ancestors(MCTSNode* node_to_keep);
    void prune_children();
    void filicide();
    void expand();
    void backpropagate(const Board &board);
    void guarded_delete();
    grid_coord get_move() const;
    policy_vec get_policy() const;
    MCTSNode(const Board &board, MCTSNode *parent);
};

float transposition_hitrate();
float transposition_size();
void mcts(MCTSNode *node, int num_iterations);
void parallel_mcts(MCTSNode *node, int num_iterations);
#endif