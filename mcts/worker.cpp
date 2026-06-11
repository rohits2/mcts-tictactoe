#include "worker.h"
#include <chrono>

void MCTSBackgroundWorker::do_work() {
  while (true) {
    board_lock.lock();
    bool dirty = board_dirty;
    board_dirty = false;
    Board current = board;
    board_lock.unlock();

    // On a board change (or when we have blown the memory budget) collapse the
    // branches that are no longer reachable, so both search effort and memory
    // stay concentrated on the live position.
    if (dirty || tree.transposition_size() > max_tree_size) {
      tree.prune_feather(current);
    }

    // Always make progress on the current root. This is what guarantees a
    // thread blocked in get_move() eventually sees enough visits to return.
    std::shared_ptr<MCTSNode> node = tree.get_node(current, nullptr);
    tree.mcts(node, iters_per_step);

    progress_cv.notify_all();
  }
}

void MCTSBackgroundWorker::set_board(const Board &new_board) {
  std::lock_guard<std::mutex> guard(board_lock);
  board = new_board;
  board_dirty = true;
}

grid_coord MCTSBackgroundWorker::get_move(const Board &board) {
  std::shared_ptr<MCTSNode> node = tree.get_node(board, nullptr);

  // Block until the worker has run at least MIN_VISITS_PER_MOVE additional
  // rollouts from this position. Without this we would commit to a move on a
  // tree the background search has barely (or never) looked at. The wait_for
  // timeout makes the wait robust to a missed notify and to the benign race on
  // reading visits.
  unsigned target = node->visits + MIN_VISITS_PER_MOVE;
  std::unique_lock<std::mutex> lk(progress_lock);
  while (node->visits < target) {
    progress_cv.wait_for(lk, std::chrono::milliseconds(50));
  }
  return node->get_move();
}

policy_vec MCTSBackgroundWorker::get_policy(const Board &board) { return tree.get_node(board, nullptr)->get_policy(); }

float MCTSBackgroundWorker::get_value(const Board &board) { return tree.get_node(board, nullptr)->Q(); }

// Win / tie probabilities for the player to move, read straight from the node's
// visit counts (wins and ties are accumulated from the mover's perspective in
// MCTSNode::backpropagate). get_value() blends these into one scalar; the chart
// needs them separated to show X / O / Tie.
float MCTSBackgroundWorker::get_win_prob(const Board &board) {
  std::shared_ptr<MCTSNode> node = tree.get_node(board, nullptr);
  std::lock_guard<std::recursive_mutex> guard(node->lock);
  return node->visits > 0 ? (float)node->wins / node->visits : 0.0f;
}

float MCTSBackgroundWorker::get_tie_prob(const Board &board) {
  std::shared_ptr<MCTSNode> node = tree.get_node(board, nullptr);
  std::lock_guard<std::recursive_mutex> guard(node->lock);
  return node->visits > 0 ? (float)node->ties / node->visits : 0.0f;
}

int MCTSBackgroundWorker::transposition_table_size() const { return tree.transposition_table.size(); }
