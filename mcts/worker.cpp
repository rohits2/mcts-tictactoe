#include "worker.h"

void MCTSBackgroundWorker::do_work() {
  while (true) {
    while (tree.transposition_size() < max_tree_size) {
      printf("Tree transposition size %d max %d iters %d\n", tree.transposition_size(), max_tree_size, iters_per_step);
      board_lock.lock();
      std::shared_ptr<MCTSNode> node = tree.get_node(board, nullptr);
      board_lock.unlock();
      tree.mcts(node, iters_per_step);
    }
    board_change_semaphore.acquire();
    board_lock.lock();
    tree.prune_feather(board);
    board_lock.unlock();
  }
}

void MCTSBackgroundWorker::set_board(const Board &new_board) {
  board_lock.lock();
  board = new_board;
  board_change_semaphore.release();
  board_lock.unlock();
}

grid_coord MCTSBackgroundWorker::get_move(const Board &board) { return tree.get_node(board, nullptr)->get_move(); }

policy_vec MCTSBackgroundWorker::get_policy(const Board &board) { return tree.get_node(board, nullptr)->get_policy(); }

float MCTSBackgroundWorker::get_value(const Board &board) { return tree.get_node(board, nullptr)->Q(); }
int MCTSBackgroundWorker::transposition_table_size() const { return tree.transposition_table.size(); }
