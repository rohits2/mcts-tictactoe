#include "mcts.h"
#ifndef PROC_COUNT
#define PROC_COUNT 1
#endif

const float C = 1.44;
const float TIE_REWARD = 0.5;
const float inf = std::numeric_limits<float>::infinity();

ThreadPool::ThreadPool(unsigned num_threads) {
  for (unsigned i = 0; i < num_threads; i++) {
    threads_.emplace_back([this] { worker_loop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lk(mtx_);
    stop_ = true;
  }
  cv_.notify_all();
  for (std::thread &t : threads_) {
    t.join();
  }
}

void ThreadPool::worker_loop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lk(mtx_);
      cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
      if (stop_ && tasks_.empty()) {
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task();
  }
}

// Fan `count` task indices across the pool and the calling thread, blocking
// until all have run. NOT reentrant: a task must not itself call parallel_for
// (the search never does -- only the single worker thread submits batches).
void ThreadPool::parallel_for(int count, const std::function<void(int)> &fn) {
  if (count <= 0) {
    return;
  }
  if (threads_.empty()) { // degenerate single-threaded build: just run inline
    for (int i = 0; i < count; i++) {
      fn(i);
    }
    return;
  }
  // remaining/done_* live on this stack frame; parallel_for blocks until every
  // task has finished, so the task closures may safely capture them by reference.
  std::atomic<int> remaining{count};
  std::mutex done_mtx;
  std::condition_variable done_cv;
  {
    std::lock_guard<std::mutex> lk(mtx_);
    for (int i = 0; i < count; i++) {
      tasks_.push([&fn, i, &remaining, &done_mtx, &done_cv] {
        fn(i);
        // Notify under the mutex so the waiter can't destroy done_cv mid-notify.
        if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
          std::lock_guard<std::mutex> dlk(done_mtx);
          done_cv.notify_one();
        }
      });
    }
  }
  cv_.notify_all();
  // The submitting thread pitches in instead of blocking idle.
  for (;;) {
    std::function<void()> task;
    {
      std::lock_guard<std::mutex> lk(mtx_);
      if (tasks_.empty()) {
        break;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    task();
  }
  std::unique_lock<std::mutex> lk(done_mtx);
  done_cv.wait(lk, [&] { return remaining.load(std::memory_order_acquire) == 0; });
}

// The pool keeps PROC_COUNT-1 threads; the thread that calls parallel_for joins
// in as the PROC_COUNT-th worker, so a batch runs PROC_COUNT-wide without an
// idle submitter. A single-core build gets an empty pool and runs inline.
MCTSTree::MCTSTree() : pool_(PROC_COUNT > 1 ? PROC_COUNT - 1 : 0) {
  // Pre-size the bucket array so a table that grows toward the search's node count
  // does not pay the ~16 incremental rehashes it otherwise would. A modest hint
  // keeps small searches cheap while still covering the common case.
  transposition_table.reserve(1 << 16);
}

std::shared_ptr<MCTSNode> MCTSTree::get_node(const Board &new_board, std::shared_ptr<MCTSNode> new_parent) {
  tree_lock.lock();
  total_lookups++;
  // Single hash + single bucket probe. The bucket holds every node whose board
  // hashes to `key`; we identify the real match with operator== (so a hash
  // collision can never alias two different positions) and reap expired entries
  // as we pass them.
  uint64_t key = new_board.hash();
  auto range = transposition_table.equal_range(key);
  for (auto it = range.first; it != range.second;) {
    std::shared_ptr<MCTSNode> node = it->second.wp.lock();
    if (!node) {
      it = transposition_table.erase(it);
      continue;
    }
    if (node->board == new_board) {
      // The UI thread only ever calls get_node(board, nullptr), so the nullptr
      // short-circuit lets it return without touching node->parents/node->lock
      // at all. A real parent link (only the search threads pass one) is made
      // under the node lock -- held here while we already hold tree_lock, i.e.
      // in the global tree_lock -> node_lock order -- so concurrent search
      // threads adding parents (here) and reading them (in U()) never race.
      if (new_parent != nullptr) {
        std::lock_guard<std::recursive_mutex> nguard(node->lock);
        if (node->parents.size() == 0) {
          auto itr = std::find(roots.begin(), roots.end(), node);
          if (itr != roots.end()) {
            roots.erase(itr);
          }
        }
        node->parents.push_back(new_parent);
      }
      total_hits++; // under tree_lock, like total_lookups
      tree_lock.unlock();
      return node;
    }
    ++it; // same hash, different board: a genuine collision, keep probing
  }
  std::shared_ptr<MCTSNode> node = std::make_shared<MCTSNode>(new_board, new_parent, this);
  transposition_table.insert({key, NodeRef{node, node.get()}});
  if (new_parent == nullptr) {
    roots.push_back(node);
  }
  tree_lock.unlock();
  return node;
}

std::shared_ptr<MCTSNode> MCTSTree::find_node(const Board &new_board) {
  // Read-only counterpart to get_node: looks the position up without inserting a
  // node or pushing a root, so the polling/stats UI never mutates the tree.
  std::lock_guard<std::recursive_mutex> guard(tree_lock);
  uint64_t key = new_board.hash();
  auto range = transposition_table.equal_range(key);
  for (auto it = range.first; it != range.second; ++it) {
    std::shared_ptr<MCTSNode> node = it->second.wp.lock();
    if (node && node->board == new_board) {
      return node;
    }
  }
  return nullptr;
}

// Alpha-beta pruning over the already-built search tree. Rather than the old
// visit-count / Q-threshold heuristics, this discards exactly the branches a
// minimax searcher would never have to look at: it runs alpha-beta with
// best-first move ordering over the live subtree and collapses (filicides) the
// children cut off at each node, freeing their descendants while keeping the
// move itself and its statistics for the UI and for get_move(). `margin`
// relaxes every cutoff so that moves within `margin` win-probability of optimal
// are retained instead of collapsing onto a single principal variation.
void MCTSTree::prune_alpha_beta(std::shared_ptr<MCTSNode> node, float margin) {
  // Snapshot the root's children, then prune each child's subtree in parallel.
  // The root and its direct children are always kept (with margin > 0 the root
  // level never cuts), so there is no cross-child alpha-beta dependency to
  // serialise: each subtree is pruned independently with the full [0, 1] window
  // and its own memo/done. Using the full window per child is at worst slightly
  // less aggressive than the sequential sweep (a wider window prunes less, never
  // wrongly). We deliberately do NOT hold tree_lock across this: the worker
  // threads' filicides drop nodes whose destructors take tree_lock themselves,
  // so holding it here would deadlock against the threads we join below.
  node->lock.lock();
  bool leaf = !node->expanded || node->children.empty();
  std::vector<std::shared_ptr<MCTSNode>> kids;
  if (!leaf) {
    kids.assign(node->children.begin(), node->children.end());
  }
  node->lock.unlock();
  if (kids.empty()) {
    return;
  }

  // One task per root child, dynamically scheduled by the pool so the (typically
  // huge) principal-variation subtree doesn't stall the others. Pass 1: exact
  // negamax values for the subtree, memoised so a node shared via transposition
  // is scored once. Pass 2: the alpha-beta sweep that prunes using them.
  // memo/done are per-subtree, so a node shared across two root children is
  // pruned by whichever task reaches it first.
  pool_.parallel_for((int)kids.size(), [&kids, margin](int i) {
    std::unordered_map<const MCTSNode *, float> memo;
    std::unordered_set<const MCTSNode *> done;
    kids[i]->minimax_value(memo);
    kids[i]->alpha_beta_prune(0.0f, 1.0f, margin, memo, done);
  });
}

void MCTSTree::prune_alpha_beta(const Board &board, float margin) {
  std::shared_ptr<MCTSNode> node = get_node(board, nullptr);
  prune_alpha_beta(node, margin);
}

char MCTSTree::proven_winner(std::shared_ptr<MCTSNode> node) {
  std::unordered_map<const MCTSNode *, char> memo;
  return node->proven_winner(memo);
}

void MCTSTree::prune_feather(std::shared_ptr<MCTSNode> node) { node->prune_ancestors(); }

void MCTSTree::prune_feather(const Board &board) {
  std::shared_ptr<MCTSNode> node = get_node(board, nullptr);
  prune_feather(node);
}

void MCTSTree::prune_soft(std::shared_ptr<MCTSNode> node) {
  node->prune_ancestors();
  prune_alpha_beta(node);
}

void MCTSTree::prune_soft(const Board &board) {
  std::shared_ptr<MCTSNode> node = get_node(board, nullptr);
  prune_soft(node);
}

float MCTSTree::transposition_hitrate() {
  if (total_lookups == 0)
    return 1;
  return total_hits / ((float)total_lookups);
}

int MCTSTree::transposition_size() {
  // size() concurrent with a search thread's insert/erase is a data race on the
  // container; read it under tree_lock like every other table access.
  std::lock_guard<std::recursive_mutex> guard(tree_lock);
  return transposition_table.size();
}
long long MCTSTree::purges() { return total_fillicides; }

MCTSNode::MCTSNode(const Board &new_board, std::shared_ptr<MCTSNode> new_parent, MCTSTree *host) {
  board = new_board;
  tree = host;
  parents.push_back(new_parent);
  // The legal-move list is recomputed on demand (in expand(), and in
  // get_move()/get_policy()) rather than stored: a never-expanded frontier node
  // then costs nothing for it.
}

float MCTSNode::Q() {
  lock.lock(); //
  float sum = wins + TIE_REWARD * ties;
  float res = sum / (1.0f + visits);
  lock.unlock();
  return res;
}

float MCTSNode::parent_Q() {
  unsigned losses = visits - wins - ties;
  float loss = losses / (1.0f + visits);
  float tie = TIE_REWARD * ties / (1.0f + visits);
  return loss + tie;
}

float MCTSNode::U() {
  unsigned parent_visit_count = 0;
  // parents is shared mutable state under parallel search (search threads add
  // links via get_node and reap expired ones here), so iterate it under the
  // node lock. The parents' visit counts are atomics, read without their locks.
  lock.lock();
  for (int i = 0; i < (int)parents.size(); i++) {
    auto parent = parents[i];
    if (parent.expired()) {
      parents.erase(parents.begin() + i);
      i--;
      continue;
    }
    parent_visit_count += parent.lock()->visits;
  }
  lock.unlock();
  return C * sqrt((float)parent_visit_count) / (1.0 + visits);
}

float MCTSNode::PUCT() { return Q() + U(); }

grid_coord MCTSNode::get_move() const {
  float best_Q = inf;
  int best_visits = 0;
  grid_coord best_move = {-1, -1, -1, -1};
  if (!expanded) {
    return best_move;
  }
  lock.lock();
  // children[i] was created from get_valid_moves()[i] in expand(), in that exact
  // order, and the order is never permuted (alpha-beta pruning sorts a copy), so
  // recompute the move list here rather than storing it on every node.
  std::vector<grid_coord> moves = board.get_valid_moves();
  for (int i = 0; i < (int)children.size() && i < (int)moves.size(); i++) {
    std::shared_ptr<MCTSNode> child = children[i];
    float Q = child->Q();
    if (Q < best_Q) {
      best_Q = Q;
      best_visits = child->visits;
      best_move = moves[i];
    } else if (Q == best_Q && child->visits > best_visits) {
      best_Q = Q;
      best_visits = child->visits;
      best_move = moves[i];
    }
  }
  lock.unlock();
  return best_move;
}

policy_vec MCTSNode::get_policy() const {
  policy_vec vec;
  if (!expanded) {
    return vec;
  }
  lock.lock();
  std::vector<grid_coord> moves = board.get_valid_moves();
  for (int ind = 0; ind < (int)children.size() && ind < (int)moves.size(); ind++) {
    std::shared_ptr<MCTSNode> child = children[ind];
    int i = moves[ind].m_i * 3 + moves[ind].i;
    int j = moves[ind].m_j * 3 + moves[ind].j;
    vec.policy[i][j] = 1 - child->Q() + 0.00001;
  }
  lock.unlock();
  return vec;
}

std::shared_ptr<MCTSNode> MCTSNode::max_PUCT() {
  float best_PUCT = -inf;
  std::shared_ptr<MCTSNode> best_node = nullptr;
  lock.lock();
  for (std::shared_ptr<MCTSNode> child : children) {
    float PUCT = (1 - child->Q()) + child->U();
    if (PUCT > best_PUCT) {
      best_PUCT = PUCT;
      best_node = child;
    }
  }
  lock.unlock();
  return best_node;
}

std::vector<std::shared_ptr<MCTSNode>> MCTSNode::select() {
  std::vector<std::shared_ptr<MCTSNode>> path;
  path.reserve(64);
  std::shared_ptr<MCTSNode> cur_node = shared_from_this();
  while (cur_node->expanded) {
    std::shared_ptr<MCTSNode> new_node = cur_node->max_PUCT();
    // Defensive: an expanded node should always have a child, but if a
    // concurrent collapse ever left it childless, stop here rather than
    // dereference a null reply on the next loop.
    if (new_node == nullptr) {
      break;
    }
    path.push_back(cur_node);
    cur_node->visits++; // atomic
    cur_node = new_node;
  };
  path.push_back(cur_node);
  cur_node->visits++; // atomic
  return path;
}

void MCTSNode::prune_ancestors() { prune_ancestors(shared_from_this()); }

// Negamax value of this subtree from the perspective of the player to move
// here, in [0, 1] (1 == win for the mover, 0.5 == tie, 0 == loss). Terminal
// positions score exactly; a non-terminal frontier node falls back to the MCTS
// estimate Q(). Internal nodes pick the best reply: V = max over children of
// (1 - V(child)), where the "1 -" flips into the opponent's perspective.
// Memoised on the node pointer so each node in the transposition DAG is scored
// once, keeping the pass O(nodes).
float MCTSNode::minimax_value(std::unordered_map<const MCTSNode *, float> &memo) {
  auto cached = memo.find(this);
  if (cached != memo.end()) {
    return cached->second;
  }
  lock.lock();
  bool leaf = !expanded || children.empty();
  std::vector<std::shared_ptr<MCTSNode>> kids;
  if (!leaf) {
    kids.assign(children.begin(), children.end());
  }
  lock.unlock();

  float value;
  if (leaf) {
    char winner = board.game_winner();
    if (winner == PLAYER_TIE) {
      value = TIE_REWARD;
    } else if (winner == PLAYER_NONE) {
      value = Q(); // unexpanded frontier: trust the rollout estimate
    } else {
      // A finished game was won by whoever just moved, i.e. the opponent of the
      // player to move here, so this node is a loss for the mover.
      value = (winner == board.player) ? 1.0f : 0.0f;
    }
  } else {
    value = -inf;
    for (std::shared_ptr<MCTSNode> &child : kids) {
      value = std::max(value, 1.0f - child->minimax_value(memo));
    }
  }
  memo[this] = value;
  return value;
}

// Conclusively-proven winner of this subtree, from a pure game-theoretic view:
// PLAYER_X / PLAYER_O if that player can force a win against any defence,
// PLAYER_TIE if best play is a forced draw, or PLAYER_NONE if the outcome is not
// yet proven (an unexpanded frontier node counts as unknown -- its rollout
// estimate is a guess, not a proof). The mover here picks the best reply, so it
// is a proven win for the mover as soon as ONE child is a proven win for it
// (short-circuit); a proven loss only once EVERY child is proven and none
// favours the mover. Memoised over the transposition DAG.
char MCTSNode::proven_winner(std::unordered_map<const MCTSNode *, char> &memo) {
  auto cached = memo.find(this);
  if (cached != memo.end()) {
    return cached->second;
  }
  char winner = board.game_winner();
  if (winner != PLAYER_NONE) { // terminal: the result is exact
    memo[this] = winner;
    return winner;
  }
  lock.lock();
  bool leaf = !expanded || children.empty();
  std::vector<std::shared_ptr<MCTSNode>> kids;
  if (!leaf) {
    kids.assign(children.begin(), children.end());
  }
  lock.unlock();
  if (leaf) { // unexpanded frontier: outcome unknown, not proven
    memo[this] = PLAYER_NONE;
    return PLAYER_NONE;
  }

  char mover = board.player;
  char opponent = (mover == PLAYER_X) ? PLAYER_O : PLAYER_X;
  bool any_unknown = false;
  bool any_tie = false;
  for (std::shared_ptr<MCTSNode> &child : kids) {
    char r = child->proven_winner(memo);
    if (r == mover) { // a move that forces the mover's win -- done
      memo[this] = mover;
      return mover;
    }
    if (r == PLAYER_NONE) {
      any_unknown = true;
    } else if (r == PLAYER_TIE) {
      any_tie = true;
    }
    // r == opponent: a losing line the mover simply won't choose.
  }
  // No forced win for the mover. If any reply is still unproven the result is
  // open; otherwise the mover steers to the best proven outcome it can (a draw
  // if available, else the forced loss).
  char result = any_unknown ? PLAYER_NONE : (any_tie ? PLAYER_TIE : opponent);
  memo[this] = result;
  return result;
}

// Alpha-beta sweep that prunes the children a minimax searcher would never need
// to examine. `memo` holds the exact values from minimax_value(); `done` makes
// each node in the DAG prune at most once. Children are visited best-first so
// cutoffs land as early (and prune as much) as possible. `margin` widens the
// cutoff test so near-optimal siblings (within `margin` of mattering) are kept
// rather than collapsed. A filicided child is collapsed to a leaf -- its move
// and statistics survive, only its descendants are freed.
void MCTSNode::alpha_beta_prune(float alpha, float beta, float margin,
                                std::unordered_map<const MCTSNode *, float> &memo,
                                std::unordered_set<const MCTSNode *> &done) {
  if (!done.insert(this).second) {
    return;
  }
  // Snapshot the children best-first under the lock, then release it: the
  // recursion and the filicides below take other nodes' locks, and holding this
  // node's lock across them would serialise whole branches and nest locks. We
  // copy (never reorder the real children/moves vectors, whose shared index
  // correspondence get_move/get_policy rely on). Best reply == smallest child
  // value (== largest 1 - value for this player).
  lock.lock();
  bool leaf = !expanded || children.empty();
  std::vector<std::shared_ptr<MCTSNode>> ordered;
  if (!leaf) {
    ordered.assign(children.begin(), children.end());
  }
  lock.unlock();
  if (leaf) {
    return;
  }
  std::sort(ordered.begin(), ordered.end(),
            [&memo](const std::shared_ptr<MCTSNode> &a, const std::shared_ptr<MCTSNode> &b) {
              return memo.at(a.get()) < memo.at(b.get());
            });

  float value = -inf;
  size_t cut_from = ordered.size();
  for (size_t i = 0; i < ordered.size(); i++) {
    // Recurse into the reply we keep, pruning inside it under the negated
    // window (the opponent's alpha/beta are 1 - our beta/alpha).
    ordered[i]->alpha_beta_prune(1.0f - beta, 1.0f - alpha, margin, memo, done);
    value = std::max(value, 1.0f - memo.at(ordered[i].get()));
    alpha = std::max(alpha, value);
    // Relaxed beta cutoff: only discard the rest once the line we keep is ahead
    // by more than `margin`, so equally-good / near-optimal replies survive.
    if (alpha >= beta + margin) {
      cut_from = i + 1;
      break;
    }
  }
  for (size_t i = cut_from; i < ordered.size(); i++) {
    ordered[i]->filicide();
  }
}

void MCTSNode::filicide() {
  lock.lock();
  if (!expanded) {
    lock.unlock();
    return;
  }
  children.clear();
  expanded = false;
  lock.unlock();
}

void MCTSNode::prune_ancestors(std::shared_ptr<const MCTSNode> node_to_keep) {
  lock.lock();
  if (shared_from_this() != node_to_keep) {
    for (std::shared_ptr<MCTSNode> child : children) {
      if (child == node_to_keep) {
        continue;
      }
      child->filicide();
    }
  }
  // Snapshot the live parents (reaping expired links) under the lock, then
  // recurse without holding it: parents is shared with the search threads, and
  // recursing while holding the lock would also nest descendant->ancestor node
  // locks against the ancestor->descendant order the rest of the tree uses.
  std::vector<std::shared_ptr<MCTSNode>> live_parents;
  for (int i = 0; i < (int)parents.size(); i++) {
    if (parents[i].expired()) {
      parents.erase(parents.begin() + i);
      i--;
      continue;
    }
    live_parents.push_back(parents[i].lock());
  }
  lock.unlock();
  for (std::shared_ptr<MCTSNode> &parent : live_parents) {
    parent->prune_ancestors(shared_from_this());
  }
}

void MCTSNode::expand() {
  visits++; // atomic
  // Claim expansion under the lock so exactly one thread builds the children.
  lock.lock();
  if (expanded || expanding) {
    lock.unlock();
    return;
  }
  expanding = true;
  lock.unlock();

  // Build the children WITHOUT holding the node lock: get_node() takes tree_lock
  // (and the child's node lock), and holding our own node lock across that would
  // invert the global tree_lock -> node_lock order and risk deadlock.
  std::vector<grid_coord> moves = board.get_valid_moves();
  std::vector<std::shared_ptr<MCTSNode>> new_children;
  new_children.reserve(moves.size());
  for (grid_coord move : moves) {
    Board new_board(board);
    new_board.move(move);
    new_children.push_back(tree->get_node(new_board, shared_from_this()));
  }

  // Publish atomically: install the children, then flip `expanded` so no other
  // thread ever sees this node expanded with an empty child list.
  lock.lock();
  children = std::move(new_children);
  expanded = !children.empty();
  expanding = false;
  lock.unlock();
}

void MCTSNode::backpropagate(const Board &board, const std::vector<std::shared_ptr<MCTSNode>> &path) {
  int winner = board.game_winner();
  for (const std::shared_ptr<MCTSNode> &node : path) {
    node->lock.lock();
    if (winner == node->board.player) {
      node->wins += 1;
    } else if (winner == PLAYER_TIE) {
      node->ties += 1;
    }
    node->lock.unlock();
  }
}

Board simulate(const Board &board) {
  Board new_board(board);
  grid_coord move;
  while (new_board.game_winner() == PLAYER_NONE) {
    // A non-terminal position should always have a legal move; bail rather than
    // spin if a degenerate (e.g. externally-supplied) state ever has none.
    if (!new_board.random_move(move)) {
      break;
    }
    new_board.move(move);
  }
  return new_board;
}

MCTSNode::~MCTSNode() {
  // Erase exactly this node's transposition entry. The table is shared with the
  // search thread, so take tree_lock (recursive: a destructor can fire while a
  // prune already holds it on this thread). wp is expired by now, so identify our
  // entry by raw pointer among any bucket-mates that share the hash.
  std::lock_guard<std::recursive_mutex> guard(tree->tree_lock);
  tree->total_fillicides++;
  uint64_t key = board.hash();
  auto range = tree->transposition_table.equal_range(key);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second.self == this) {
      tree->transposition_table.erase(it);
      break;
    }
  }
}

void MCTSTree::mcts(std::shared_ptr<MCTSNode> node, int num_iterations) {
  for (int it = 0; it < num_iterations; it++) {
    std::vector<std::shared_ptr<MCTSNode>> path = node->select();
    std::shared_ptr<MCTSNode> leaf = path.back();
    auto board = simulate(leaf->board);
    leaf->backpropagate(board, path);
    if (leaf->board.game_winner() == PLAYER_NONE) {
      leaf->expand();
    }
  }
  total_iterations += num_iterations;
}

void MCTSTree::mcts(const Board &board, int num_iterations) {
  std::shared_ptr<MCTSNode> node = get_node(board, nullptr);
  mcts(node, num_iterations);
}

void MCTSTree::parallel_mcts(std::shared_ptr<MCTSNode> node, int num_iterations) {
  if (num_iterations <= 0) {
    return;
  }
  // Split the rollouts into one block per effective worker (pool threads + the
  // calling thread) and run them on the persistent pool -- no per-batch thread
  // creation. The first `extra` blocks carry the remainder, so the blocks sum to
  // exactly num_iterations.
  int workers = (int)pool_.size() + 1;
  int n_tasks = std::min(num_iterations, workers);
  int base = num_iterations / n_tasks;
  int extra = num_iterations % n_tasks;
  pool_.parallel_for(n_tasks, [this, node, base, extra](int t) {
    int block = base + (t < extra ? 1 : 0);
    if (block > 0) {
      this->mcts(node, block);
    }
  });
}

void MCTSTree::parallel_mcts(const Board &board, int num_iterations) {
  std::shared_ptr<MCTSNode> node = get_node(board, nullptr);
  parallel_mcts(node, num_iterations);
}