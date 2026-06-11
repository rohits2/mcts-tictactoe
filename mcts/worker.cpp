#include "worker.h"
#include <chrono>

void MCTSBackgroundWorker::do_work() {
  while (true) {
    board_lock.lock();
    bool dirty = board_dirty;
    board_dirty = false;
    Board current = board;
    board_lock.unlock();

    // On a board change, collapse the branches that are no longer reachable so
    // both search effort and memory stay concentrated on the live position.
    if (dirty) {
      tree.prune_feather(current);
    }

    // If the live subtree itself has blown the memory budget, reachability
    // pruning can't help (it is all reachable). Use alpha-beta to discard the
    // branches that cannot change optimal play from here.
    if (tree.transposition_size() > max_tree_size) {
      tree.prune_alpha_beta(current);
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

// Value / win / tie probabilities for the player to move, read straight from the
// node's visit counts (wins and ties are accumulated from the mover's perspective
// in MCTSNode::backpropagate). These use the read-only find_node so querying many
// positions (e.g. the per-move hints) never inserts or roots a node; an
// un-searched position reads as 0.
float MCTSBackgroundWorker::get_value(const Board &board) {
  std::shared_ptr<MCTSNode> node = tree.find_node(board);
  return node ? node->Q() : 0.0f;
}

float MCTSBackgroundWorker::get_win_prob(const Board &board) {
  std::shared_ptr<MCTSNode> node = tree.find_node(board);
  if (!node) return 0.0f;
  std::lock_guard<std::recursive_mutex> guard(node->lock);
  return node->visits > 0 ? (float)node->wins / node->visits : 0.0f;
}

float MCTSBackgroundWorker::get_tie_prob(const Board &board) {
  std::shared_ptr<MCTSNode> node = tree.find_node(board);
  if (!node) return 0.0f;
  std::lock_guard<std::recursive_mutex> guard(node->lock);
  return node->visits > 0 ? (float)node->ties / node->visits : 0.0f;
}

int MCTSBackgroundWorker::transposition_table_size() {
  // Read under the lock: size() concurrent with the search thread's insert/erase
  // would otherwise be a data race on the container.
  std::lock_guard<std::recursive_mutex> guard(tree.tree_lock);
  return tree.transposition_table.size();
}

// Non-blocking: read the current best move / visit count straight from the live
// search tree (the worker keeps deepening it in the background). find_node is a
// pure read -- it never inserts or roots a node -- so polling cannot mutate the
// tree or leak orphan roots. An absent position reads as "no move / 0 visits".
grid_coord MCTSBackgroundWorker::peek_move(const Board &board) {
  std::shared_ptr<MCTSNode> node = tree.find_node(board);
  return node ? node->get_move() : grid_coord{-1, -1, -1, -1};
}

unsigned MCTSBackgroundWorker::node_visits(const Board &board) {
  std::shared_ptr<MCTSNode> node = tree.find_node(board);
  return node ? node->visits.load() : 0u;
}

long long MCTSBackgroundWorker::rollouts() const { return tree.total_iterations; }
