#include "mcts.h"

const float C = 1.4;
const float inf = std::numeric_limits<float>::infinity();

shared_mutex tree_lock;
unordered_map<Board, MCTSNode *> transposition_table;
long long total_lookups = 0;
long long total_hits = 0;

MCTSNode::MCTSNode(const Board &new_board, MCTSNode *new_parent) {
    board = new_board;
    parent = new_parent;
    moves = board.get_valid_moves();
}

MCTSNode *MCTSNode::get_node(const Board &new_board, MCTSNode *new_parent) {
    tree_lock.lock();
    total_lookups++;
    if (transposition_table.find(new_board) != transposition_table.end()) {
        // printf("Found state in transposition table!\n");
        MCTSNode *node = transposition_table[new_board];
        if (new_parent != NULL) {
            node->parent = new_parent;
            node->ref_count++;
        }
        tree_lock.unlock();
        total_hits++;
        return node;
    }
    // printf("Did not find state in transposition table!\n");
    MCTSNode *node = new MCTSNode(new_board, new_parent);
    node->ref_count++;
    auto entry = pair<Board, MCTSNode *>(new_board, node);
    transposition_table.insert(entry);
    tree_lock.unlock();
    return node;
}

float MCTSNode::Q() {
    lock.lock();
    float sum = 1.0f + (float)visits;
    float res = rewards / (1.0 + visits);
    lock.unlock();
    return res;
}

float MCTSNode::U() { return C * sqrt((float)parent->visits) / (1.0 + visits); }

float MCTSNode::PUCT() { return Q() + U(); }

grid_coord MCTSNode::get_move() const {
    float best_Q = -inf;
    grid_coord best_move = {-1, -1, -1, -1};
    if (!expanded) {
        return best_move;
    }
    lock.lock();
    for (int i = 0; i < children.size(); i++) {
        MCTSNode *child = children[i];
        float Q = 1 - child->Q();
        if (Q > best_Q) {
            best_Q = Q;
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
    for (int ind = 0; ind < children.size(); ind++) {
        MCTSNode *child = children[ind];
        int i = moves[ind].m_i * 3 + moves[ind].i;
        int j = moves[ind].m_i * 3 + moves[ind].i;
        vec.policy[i][j] = 1 - child->Q() + 0.00001;
    }
    lock.unlock();
    return vec;
}

MCTSNode::~MCTSNode() {
    //printf("Destructor %p (expanded %d)\n", this, expanded);
    tree_lock.lock();
    if (transposition_table.find(this->board) == transposition_table.end()) {
        printf("[CRITICAL] Double delete of node detected! (entry does not exist in transposition table)!");
    }
    transposition_table.erase(board);
    tree_lock.unlock();
    if (!expanded) {
        return;
    }
    for (MCTSNode *child : children) {
        child->guarded_delete();
    }
}

MCTSNode *MCTSNode::max_PUCT() {
    float best_PUCT = -inf;
    MCTSNode *best_node = NULL;
    lock.lock();
    for (MCTSNode *child : children) {
        float PUCT = (1 - child->Q()) + child->U();
        if (PUCT > best_PUCT) {
            best_PUCT = PUCT;
            best_node = child;
        }
    }
    lock.unlock();
    return best_node;
}

MCTSNode *MCTSNode::select() {
    MCTSNode *node = this;
    while (node->expanded) {
        MCTSNode *new_node = node->max_PUCT();
        node->lock.lock();
        node->visits++;
        node->lock.unlock();
        node = new_node;
    };
    return node;
}

void MCTSNode::prune_ancestors() {
    lock.lock();
    if (parent != NULL) {
        parent->prune_ancestors(this);
    }
    lock.unlock();
}
void MCTSNode::prune_children() {
    lock.lock();
    vector<float> Qs;
    for (auto child : children) {
        Qs.push_back(1 - child->Q());
    }
    for (int i = 0; i < children.size(); i++) {
        auto child = children[i];
        float QU = (1 - child->Q()) + child->Q();
        bool prunable = false;
        for (int j = 0; j < i; j++) {
            if (QU < Qs[j]) {
                prunable = true;
            }
        }
        for (int j = i + 1; j < children.size(); j++) {
            if (QU < Qs[j]) {
                prunable = true;
            }
        }
        if (prunable) {
            child->filicide();
            //printf("Deleting subtree (strictly worse)...");
        }
    }
    lock.unlock();
}

void MCTSNode::filicide() {
    lock.lock();
    if (!expanded) {
        //printf("Node ordered to commit filicide, but no children!\n");
        lock.unlock();
        return;
    }
    for (MCTSNode *child : children) {
        //printf("Child %p (expanded %d)\n", child, child->expanded);
        child->guarded_delete();
    }
    children.clear();
    expanded = false;
    lock.unlock();
}

void MCTSNode::guarded_delete() {
    lock.lock();
    ref_count--;
    if (ref_count <= 0) {
        lock.unlock();
        delete this;
    } else {
        lock.unlock();
    }
}

void MCTSNode::prune_ancestors(MCTSNode *node_to_keep) {
    lock.lock();
    if (!expanded) {
        lock.unlock();
        return;
    }
    for (MCTSNode *child : children) {
        if (child == node_to_keep) {
            continue;
        }
        //printf("Internal node committed filicide!\n");
        child->filicide();
    }
    lock.unlock();
    if (parent != NULL) {
        parent->prune_ancestors(this);
    }
}

void MCTSNode::expand() {
    lock.lock();
    visits++;
    if (expanded) {
        lock.unlock();
        return;
    }
    for (grid_coord move : moves) {
        expanded = true;
        Board new_board(board);
        new_board.move(move);
        MCTSNode *new_node = get_node(new_board, this);
        children.push_back(new_node);
    }
    lock.unlock();
}

void MCTSNode::backpropagate(const Board &board) {
    int winner = board.game_winner();
    MCTSNode *node = this;
    while (node != NULL) {
        node->lock.lock();
        if (winner == node->board.player) {
            node->rewards += 1;
        } else if (winner == PLAYER_TIE) {
            node->rewards += 0.5;
        }
        node->lock.unlock();
        node = node->parent;
    }
}

Board simulate(const Board &board) {
    Board new_board(board);
    while (new_board.game_winner() == PLAYER_NONE) {
        vector<grid_coord> s_moves = new_board.get_valid_moves();
        int rnum = rand() % (int)s_moves.size();
        grid_coord move = s_moves[rnum];
        new_board.move(move);
    }
    return new_board;
}

float transposition_hitrate() { return ((float)total_hits) / total_lookups; }

float transposition_size() { return transposition_table.size(); }

void mcts(MCTSNode *node, int num_iterations) {
    for (int it = 0; it < num_iterations; it++) {
        MCTSNode *leaf = node->select();
        auto board = simulate(leaf->board);
        leaf->backpropagate(board);
        if (leaf->board.game_winner() == PLAYER_NONE) {
            leaf->expand();
        }
    }
}
void parallel_mcts(MCTSNode *node, int num_iterations) {
    int remaining = num_iterations;
    int n_threads = thread::hardware_concurrency() - 1;
    n_threads = n_threads < 1 ? 1 : n_threads;
    int block_sz = num_iterations / n_threads;
    vector<thread *> workers;
    while (remaining > 0) {
        int block = min(block_sz, remaining);
        workers.push_back(new thread(mcts, node, block));
        remaining -= block;
    }
    for (thread *worker : workers) {
        worker->join();
        delete worker;
    }
}