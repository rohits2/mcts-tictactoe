#include "board.h"
#include "mcts.h"

//#define PROC_COUNT 2 // by default, build with multicore support

extern "C" int get_move(char grid[9][9], int player, int i, int j) {
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    MCTSNode* node = MCTSNode::get_node(board, NULL);
    if (PROC_COUNT == 1) {
        mcts(node, 50000);
    } else {
        parallel_mcts(node, 100000);
    }
    printf("Overall transposition hitrate: %f\n", transposition_hitrate());
    node->prune_children();
    node->prune_ancestors();
    grid_coord move = node->get_move();
    int i_move = (move.m_i << 24) | (move.m_j << 16) | (move.i << 8) | move.j;
    node->guarded_delete();
    return i_move;
}

extern "C" policy_vec get_policy(char grid[9][9], int player, int i, int j) {
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    MCTSNode* node = MCTSNode::get_node(board, NULL);
    if (PROC_COUNT == 1) {
        mcts(node, 50000);
    } else {
        parallel_mcts(node, 100000);
    }
    auto policy = node->get_policy();
    node->guarded_delete();
    return policy;
}

extern "C" long long transposition_table_size(){
    return transposition_size();
}

int x_main() {
    Board board;
    MCTSNode node(board, NULL);
    mcts(&node, 50000);
    printf("%.2f/%u\n", node.rewards, node.visits);
    grid_coord move = node.get_move();
    printf("%d, %d, %d, %d\n", move.m_i, move.m_j, move.i, move.j);
    return 0;
}