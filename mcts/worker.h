#ifndef WORKER_H
#define WORKER_H
#include "board.h"
#include "mcts.h"
#include <condition_variable>
#include <mutex>
#include <thread>

static constexpr int MAX_TREE_SIZE = 1048576;
static constexpr int ITERS_PER_STEP = 16384;
// Minimum number of fresh rollouts to accumulate on a position before
// get_move() is allowed to commit to a move. This is the knob that trades move
// latency for playing strength: too low and we move on a barely-searched tree.
static constexpr int MIN_VISITS_PER_MOVE = 100000;

class MCTSBackgroundWorker {
public:
  MCTSBackgroundWorker() : max_tree_size(MAX_TREE_SIZE), iters_per_step(ITERS_PER_STEP) {
    work_thread = std::thread(&MCTSBackgroundWorker::do_work, this);
  }
  MCTSBackgroundWorker(int max_tree_size, int iters_per_step)
      : max_tree_size(max_tree_size), iters_per_step(iters_per_step) {
    work_thread = std::thread(&MCTSBackgroundWorker::do_work, this);
  }

  void set_board(const Board &board);
  grid_coord get_move(const Board &board);
  policy_vec get_policy(const Board &board);
  float get_value(const Board &board);
  float get_win_prob(const Board &board);
  float get_tie_prob(const Board &board);
  int transposition_table_size() const;

protected:
  MCTSTree tree;
  Board board;
  int max_tree_size;
  int iters_per_step;
  std::thread work_thread;

  std::mutex board_lock;
  bool board_dirty = false;

  // Signalled by the worker after every batch of rollouts so a thread blocked
  // in get_move() can re-check whether enough search has accumulated.
  std::mutex progress_lock;
  std::condition_variable progress_cv;

  void do_work();
};

#endif
