#include "mcts.h"
#ifndef PROC_COUNT
#define PROC_COUNT 1
#endif

const float C = 1.44;
const float TIE_REWARD = 0.5;
const float inf = std::numeric_limits<float>::infinity();

MCTSTree::MCTSTree() {
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
      // Order matters: the UI thread calls get_node(board, nullptr), so testing
      // new_parent first short-circuits and never touches node->parents -- which
      // the worker thread mutates under the node lock. parents is thus only ever
      // accessed from the single search thread.
      if (new_parent != nullptr && node->parents.size() == 0) {
        auto itr = std::find(roots.begin(), roots.end(), node);
        if (itr != roots.end()) {
          roots.erase(itr);
        }
      }
      if (new_parent != nullptr) {
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
  tree_lock.lock();
  // Pass 1: exact negamax values for the whole reachable subtree, memoised so a
  // node shared via transposition is scored once (the tree is a DAG).
  std::unordered_map<const MCTSNode *, float> values;
  node->minimax_value(values);
  // Pass 2: alpha-beta sweep that prunes using those values. The full [0, 1]
  // window at the root still yields plenty of cutoffs deeper in the tree.
  std::unordered_set<const MCTSNode *> done;
  node->alpha_beta_prune(0.0f, 1.0f, margin, values, done);
  tree_lock.unlock();
}

void MCTSTree::prune_alpha_beta(const Board &board, float margin) {
  std::shared_ptr<MCTSNode> node = get_node(board, nullptr);
  prune_alpha_beta(node, margin);
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

int MCTSTree::transposition_size() { return transposition_table.size(); }
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
  for (int i = 0; i < parents.size(); i++) {
    auto parent = parents[i];
    if (parent.expired()) {
      parents.erase(parents.begin() + i);
      i--;
      continue;
    }
    parent_visit_count += parent.lock()->visits;
  }
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
    path.push_back(cur_node);
    std::shared_ptr<MCTSNode> new_node = cur_node->max_PUCT();
    cur_node->lock.lock();
    cur_node->visits++;
    cur_node->lock.unlock();
    cur_node = new_node;
  };
  path.push_back(cur_node);
  cur_node->visits++;
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
  lock.lock();
  if (!expanded || children.empty()) {
    lock.unlock();
    return;
  }
  // Order a copy of the children best-first for the player to move; never touch
  // the real children/moves vectors, whose shared index correspondence get_move
  // and get_policy rely on. Best reply == smallest child value (== largest
  // 1 - value for this player).
  std::vector<std::shared_ptr<MCTSNode>> ordered(children.begin(), children.end());
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
  lock.unlock();
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
  lock.unlock();
  for (int i = 0; i < parents.size(); i++) {
    auto wk_parent = parents[i];
    if (wk_parent.expired()) {
      parents.erase(parents.begin() + i);
      i--;
      continue;
    }
    std::shared_ptr<MCTSNode> parent = wk_parent.lock();
    parent->prune_ancestors(shared_from_this());
  }
}

void MCTSNode::expand() {
  lock.lock();
  visits++;
  if (expanded) {
    lock.unlock();
    return;
  }
  std::vector<grid_coord> moves = board.get_valid_moves();
  for (grid_coord move : moves) {
    expanded = true;
    Board new_board(board);
    new_board.move(move);
    std::shared_ptr<MCTSNode> new_node = tree->get_node(new_board, shared_from_this());
    children.push_back(new_node);
  }
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

void MCTSTree::parallel_mcts(const Board &board, int num_iterations) {
  std::shared_ptr<MCTSNode> node = get_node(board, nullptr);
  int remaining = num_iterations;
  int n_threads = PROC_COUNT;
  n_threads = n_threads < 1 ? 1 : n_threads;
  int block_sz = num_iterations / n_threads;
  std::vector<std::thread *> workers;
  while (remaining > 0) {
    int block = std::min(block_sz, remaining);
    workers.push_back(new std::thread([this, node, block] { this->mcts(node, block); }));
    remaining -= block;
  }
  for (std::thread *worker : workers) {
    worker->join();
    delete worker;
  }
}