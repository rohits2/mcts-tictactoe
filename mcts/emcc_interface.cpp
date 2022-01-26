#include "board.h"
#include "mcts.h"
#include "worker.h"

#ifndef PROC_COUNT
#define PROC_COUNT 2 // by default, build with multicore support
#endif

MCTSBackgroundWorker* worker = nullptr;

extern "C" float get_value(char grid[9][9], int player, int i, int j) {
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    
    float Q = worker->get_value(board);
    printf("Requested value for player %d, sgs (%d, %d) = %f\n", player, i, j, Q);
    return Q;
}

extern "C" int get_move(char grid[9][9], int player, int i, int j) {
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    worker->set_board(board);
    grid_coord move = worker->get_move(board);
    int i_move = (move.m_i << 24) | (move.m_j << 16) | (move.i << 8) | move.j;
    return i_move;
}

extern "C" policy_vec get_policy(char grid[9][9], int player, int i, int j) {
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    worker->set_board(board);
    auto policy = worker->get_policy(board);
    return policy;
}

extern "C" long long transposition_table_size() { return worker->transposition_table_size(); }

int main(){
    printf("Initialized new MCTSBackgroundWorker!\n");
    worker = new MCTSBackgroundWorker();
}

int test_main() {
    Board board;
    MCTSTree supertree;
    std::shared_ptr<MCTSNode> node = supertree.get_node(board, nullptr);
    supertree.mcts(board, 50000);
    printf("%u/%u\n", node->wins, node->visits);
    grid_coord move = node->get_move();
    printf("%d, %d, %d, %d\n", move.m_i, move.m_j, move.i, move.j);
    return 0;
}