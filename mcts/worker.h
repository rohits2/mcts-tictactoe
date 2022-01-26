#ifndef WORKER_H
#define WORKER_H
#include "board.h"
#include "mcts.h"
#include <mutex>
#include <semaphore>
#include <thread>

static constexpr int MAX_TREE_SIZE = 1048576;
static constexpr int ITERS_PER_STEP = 16384;

class MCTSBackgroundWorker {
public:
  MCTSBackgroundWorker() : max_tree_size(MAX_TREE_SIZE), iters_per_step(ITERS_PER_STEP){
       work_thread = std::thread(&MCTSBackgroundWorker::do_work, this); 
  }
  MCTSBackgroundWorker(int max_tree_size, int iters_per_step) : max_tree_size(max_tree_size), iters_per_step(iters_per_step){
     work_thread = std::thread(&MCTSBackgroundWorker::do_work, this); 
  }

  void set_board(const Board &board);
  grid_coord get_move(const Board &board) ;
  policy_vec get_policy(const Board &board) ;
  float get_value(const Board& board);
  int transposition_table_size() const;

protected:
  MCTSTree tree;
  Board board;
  int max_tree_size;
  int iters_per_step;
  std::thread work_thread;
  std::mutex board_lock;
  std::binary_semaphore board_change_semaphore;

  void do_work();
};

#endif