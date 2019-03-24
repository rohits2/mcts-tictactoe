#include "board.h"
#include "mcts.h"

//#define PROC_COUNT 2 // by default, build with multicore support

MCTSTree tree;

extern "C" int get_move(char grid[9][9], int player, int i, int j) {
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    auto node = tree.get_node(board, nullptr);
    if (PROC_COUNT == 1) {
        tree.mcts(board, 50000);
    } else {
        tree.mcts(board, 100000);
    }
    printf("Overall transposition hitrate: %f\n", tree.transposition_hitrate());
    printf("Overall transposition size: %d\n", tree.transposition_size());
    //node->prune_children();
    node->prune_ancestors();
    grid_coord move = node->get_move();
    int i_move = (move.m_i << 24) | (move.m_j << 16) | (move.i << 8) | move.j;
    return i_move;
}

extern "C" policy_vec get_policy(char grid[9][9], int player, int i, int j) {
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    auto node = tree.get_node(board, nullptr);
    if (PROC_COUNT == 1) {
        tree.mcts(board, 50000);
    } else {
        tree.mcts(board, 100000);
    }
    auto policy = node->get_policy();
    return policy;
}

extern "C" long long transposition_table_size(){
    return tree.transposition_table.size();
}


int main() {
    Board board;
    MCTSTree supertree;
    shared_ptr<MCTSNode> node = supertree.get_node(board, nullptr);
    supertree.mcts(board, 50000);
    printf("%u/%u\n", node->wins, node->visits);
    grid_coord move = node->get_move();
    printf("%d, %d, %d, %d\n", move.m_i, move.m_j, move.i, move.j);
    return 0;
}