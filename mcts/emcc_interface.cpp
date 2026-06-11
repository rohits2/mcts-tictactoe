#include "board.h"
#include "mcts.h"
#include "worker.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/heap.h>
#endif

#ifndef PROC_COUNT
#define PROC_COUNT 2 // by default, build with multicore support
#endif

MCTSBackgroundWorker* worker = nullptr;

// True once main() has constructed the worker. Under PROXY_TO_PTHREAD the JS
// runtime can be "initialized" before the proxied main() finishes, so the UI
// polls this before calling anything that touches the worker.
extern "C" int is_ready() { return worker != nullptr; }

extern "C" float get_value(char grid[9][9], int player, int i, int j) {
    if (!worker) return 0.0f;
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    return worker->get_value(board);
}

extern "C" float get_win_prob(char grid[9][9], int player, int i, int j) {
    if (!worker) return 0.0f;
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    return worker->get_win_prob(board);
}

extern "C" float get_tie_prob(char grid[9][9], int player, int i, int j) {
    if (!worker) return 0.0f;
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    return worker->get_tie_prob(board);
}

extern "C" int get_move(char grid[9][9], int player, int i, int j) {
    if (!worker) return -1;
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    worker->set_board(board);
    grid_coord move = worker->get_move(board);
    return (move.m_i << 24) | (move.m_j << 16) | (move.i << 8) | move.j;
}

// Tell the background worker which position to keep searching (pondering),
// without waiting for a result.
extern "C" void ponder(char grid[9][9], int player, int i, int j) {
    if (!worker) return;
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    worker->set_board(board);
}

// Non-blocking: the best move found so far for this position (packed like
// get_move). Returns -1 (unpacks to an invalid move) if the position has not
// been searched yet.
extern "C" int peek_move(char grid[9][9], int player, int i, int j) {
    if (!worker) return -1;
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    grid_coord move = worker->peek_move(board);
    return (move.m_i << 24) | (move.m_j << 16) | (move.i << 8) | move.j;
}

// Non-blocking: rollouts spent on this position so far. Returned as a double so
// the (potentially >2^31) count survives the JS number boundary intact.
extern "C" double node_visits(char grid[9][9], int player, int i, int j) {
    if (!worker) return 0;
    supergrid_coord major_tile{i, j};
    Board board(grid, player, major_tile);
    return (double)worker->node_visits(board);
}

// Cumulative MCTS rollouts across the whole search (monotonic) -- for rollouts/sec.
extern "C" double rollouts() { return worker ? (double)worker->rollouts() : 0; }

extern "C" long long transposition_table_size() { return worker ? worker->transposition_table_size() : 0; }

// Total size of the WASM heap in bytes.
extern "C" double heap_bytes() {
#ifdef __EMSCRIPTEN__
    return (double)emscripten_get_heap_size();
#else
    return 0;
#endif
}

int main(){
    printf("Initialized new MCTSBackgroundWorker!\n");
    worker = new MCTSBackgroundWorker();
}

int test_main() {
    Board board;
    MCTSTree supertree;
    std::shared_ptr<MCTSNode> node = supertree.get_node(board, nullptr);
    supertree.mcts(board, 50000);
    printf("%u/%u\n", node->wins.load(), node->visits.load());
    grid_coord move = node->get_move();
    printf("%d, %d, %d, %d\n", move.m_i, move.m_j, move.i, move.j);
    return 0;
}
