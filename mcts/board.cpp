#include "board.h"
#include <cstdlib>

/*
 * The eight winning lines of a 3x3 grid, expressed over the 9-bit local layout
 * (local = row * 3 + col): three rows, three columns, two diagonals.
 */
static const uint16_t WIN_LINES[8] = {
    0x007, 0x038, 0x1C0, // rows
    0x049, 0x092, 0x124, // columns
    0x111, 0x054,        // diagonals
};

static inline bool is_win9(uint16_t m) {
  for (int k = 0; k < 8; k++) {
    if ((m & WIN_LINES[k]) == WIN_LINES[k]) {
      return true;
    }
  }
  return false;
}

/*
 * MurmurHash3 64-bit finalizer: a strong avalanche mix used to fold the
 * bitboards into a well-distributed 64-bit hash.
 */
static inline uint64_t fmix64(uint64_t k) {
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33;
  k *= 0xc4ceb9fe1a85ec53ULL;
  k ^= k >> 33;
  return k;
}

/*
 * Given a player, find the next one.
 */
int get_next_player(int player) {
  switch (player) {
  case PLAYER_X:
    return PLAYER_O;
  case PLAYER_O:
    return PLAYER_X;
  default:
    return PLAYER_NONE;
  }
}

/*
 * Detect if supergrid coordinates are not set.
 */
bool is_unset(supergrid_coord tile) { return (tile.i == -1) && (tile.j == -1); }

/*
 * Cell value at board coordinates (g_i, g_j).
 */
char Board::cell(int g_i, int g_j) const {
  int sg = (g_i / 3) * 3 + (g_j / 3);
  int local = (g_i % 3) * 3 + (g_j % 3);
  int bit = sg * 9 + local;
  if ((x >> bit) & 1) {
    return PLAYER_X;
  }
  if ((o >> bit) & 1) {
    return PLAYER_O;
  }
  return PLAYER_NONE;
}

/*
 * Set a single cell (subgrid, local) for the given player.
 */
void Board::place(int sg, int local, char who) {
  int bit = sg * 9 + local;
  if (who == PLAYER_X) {
    x |= ((board_mask)1 << bit);
  } else {
    o |= ((board_mask)1 << bit);
  }
}

/*
 * Refresh the cached supergrid status (won/tied) for one subgrid.
 */
void Board::recompute_subgrid(int sg) {
  uint16_t x9 = (uint16_t)((x >> (sg * 9)) & 0x1FF);
  uint16_t o9 = (uint16_t)((o >> (sg * 9)) & 0x1FF);
  uint16_t bit = (uint16_t)(1u << sg);
  sx &= ~bit;
  so &= ~bit;
  sclosed &= ~bit;
  if (is_win9(x9)) {
    sx |= bit;
    sclosed |= bit;
  } else if (is_win9(o9)) {
    so |= bit;
    sclosed |= bit;
  } else if ((x9 | o9) == 0x1FF) {
    // Full with no line: a tie. The subgrid is closed but won by neither.
    sclosed |= bit;
  }
}

/*
 * Recompute the overall winner from the cached supergrid status.
 */
void Board::recompute_winner() {
  if (is_win9(sx)) {
    winner = PLAYER_X;
  } else if (is_win9(so)) {
    winner = PLAYER_O;
  } else if (sclosed == 0x1FF) {
    winner = PLAYER_TIE;
  } else {
    winner = PLAYER_NONE;
  }
}

/*
 * Recompute every subgrid's status and the overall winner from the bitboards.
 */
void Board::recompute_all() {
  sx = 0;
  so = 0;
  sclosed = 0;
  for (int sg = 0; sg < 9; sg++) {
    recompute_subgrid(sg);
  }
  recompute_winner();
}

/*
 * Create an empty board.
 */
Board::Board() { recompute_all(); }

/*
 * Build a board from a row-major char grid, the active player, and the active tile.
 */
Board::Board(const char grid[9][9], const int active_player, const supergrid_coord active_tile) {
  for (int g_i = 0; g_i < 9; g_i++) {
    for (int g_j = 0; g_j < 9; g_j++) {
      char v = grid[g_i][g_j];
      if (v == PLAYER_X || v == PLAYER_O) {
        int sg = (g_i / 3) * 3 + (g_j / 3);
        int local = (g_i % 3) * 3 + (g_j % 3);
        place(sg, local, v);
      }
    }
  }
  player = active_player;
  major_tile = active_tile;
  recompute_all();
  // Match move()'s rule: if the supplied forced subgrid is already closed, the
  // next player may move anywhere. Without this, an externally-supplied tile
  // pointing into a won/tied subgrid would be a non-terminal position with no
  // legal moves -- a state the search cannot handle.
  if (major_tile.i >= 0 && major_tile.j >= 0) {
    int sg = major_tile.i * 3 + major_tile.j;
    if ((sclosed >> sg) & 1) {
      major_tile = {.i = -1, .j = -1};
    }
  }
}

/*
 * Check if the game has a winner.
 */
char Board::game_winner() const { return winner; }

/*
 * Get the move set of the current player, given a board state.
 */
std::vector<grid_coord> Board::get_valid_moves() const {
  std::vector<grid_coord> moves;
  if (winner == PLAYER_TIE) {
    return moves;
  }
  if (is_unset(major_tile)) {
    for (int g_i = 0; g_i < 9; g_i++) {
      for (int g_j = 0; g_j < 9; g_j++) {
        int sg = (g_i / 3) * 3 + (g_j / 3);
        if ((sclosed >> sg) & 1) {
          continue;
        }
        if (cell(g_i, g_j) == PLAYER_NONE) {
          moves.push_back(grid_coord{.m_i = g_i / 3, .m_j = g_j / 3, .i = g_i % 3, .j = g_j % 3});
        }
      }
    }
  } else {
    for (int g_i = major_tile.i * 3; g_i < major_tile.i * 3 + 3; g_i++) {
      for (int g_j = major_tile.j * 3; g_j < major_tile.j * 3 + 3; g_j++) {
        if (cell(g_i, g_j) == PLAYER_NONE) {
          moves.push_back(grid_coord{.m_i = g_i / 3, .m_j = g_j / 3, .i = g_i % 3, .j = g_j % 3});
        }
      }
    }
  }
  return moves;
}

/*
 * Pick one uniformly-random legal move without allocating. Enumerates exactly the
 * same candidate set, in the same order, as get_valid_moves(), then draws a single
 * rand() index -- so it is behaviourally identical to the old
 * get_valid_moves()[rand() % size()] rollout step, just allocation-free.
 */
bool Board::random_move(grid_coord &out) const {
  grid_coord buf[81];
  int n = 0;
  if (winner == PLAYER_TIE) {
    return false;
  }
  if (is_unset(major_tile)) {
    for (int g_i = 0; g_i < 9; g_i++) {
      for (int g_j = 0; g_j < 9; g_j++) {
        int sg = (g_i / 3) * 3 + (g_j / 3);
        if ((sclosed >> sg) & 1) {
          continue;
        }
        if (cell(g_i, g_j) == PLAYER_NONE) {
          buf[n++] = grid_coord{.m_i = g_i / 3, .m_j = g_j / 3, .i = g_i % 3, .j = g_j % 3};
        }
      }
    }
  } else {
    for (int g_i = major_tile.i * 3; g_i < major_tile.i * 3 + 3; g_i++) {
      for (int g_j = major_tile.j * 3; g_j < major_tile.j * 3 + 3; g_j++) {
        if (cell(g_i, g_j) == PLAYER_NONE) {
          buf[n++] = grid_coord{.m_i = g_i / 3, .m_j = g_j / 3, .i = g_i % 3, .j = g_j % 3};
        }
      }
    }
  }
  if (n == 0) {
    return false;
  }
  out = buf[rand() % n];
  return true;
}

/*
 * Check if a move is valid, given the current board state.
 */
bool Board::is_valid_move(const grid_coord &move) const {
  int g_i = move.m_i * 3 + move.i;
  int g_j = move.m_j * 3 + move.j;
  if (cell(g_i, g_j) != PLAYER_NONE) {
    return false;
  }
  if (is_unset(major_tile)) {
    return true;
  }
  return major_tile.i == move.m_i || major_tile.j == move.m_j;
}

/*
 * Make a move and update this board.
 * If the move is not valid, this will do nothing and return false.
 */
bool Board::move(const grid_coord &move) {
  if (!is_valid_move(move)) {
    return false;
  }
  int sg = move.m_i * 3 + move.m_j;
  int local = move.i * 3 + move.j;
  place(sg, local, player);
  recompute_subgrid(sg);
  recompute_winner();
  // The next forced subgrid is the one pointed at by the move's within-cell
  // coordinates -- unless that subgrid is already closed, in which case the
  // next player may move anywhere.
  int target_sg = move.i * 3 + move.j;
  if ((sclosed >> target_sg) & 1) {
    major_tile = {.i = -1, .j = -1};
  } else {
    major_tile = {.i = move.i, .j = move.j};
  }
  player = get_next_player(player);
  return true;
}

/*
 * Pretty-print the board to stdout.
 */
void Board::print() {
  std::cout << std::endl;
  for (int i = 0; i < 9; i++) {
    if (i % 3 == 0 && i > 0) {
      std::cout << "-----------" << std::endl;
    }
    for (int j = 0; j < 9; j++) {
      if (j % 3 == 0 && j > 0) {
        std::cout << "|";
      }
      switch (cell(i, j)) {
      case PLAYER_X:
        std::cout << "X";
        break;
      case PLAYER_O:
        std::cout << "O";
        break;
      default:
        std::cout << " ";
        break;
      }
    }
    std::cout << std::endl;
  }
  std::cout << std::endl;
}

/*
 * Compare two boards for equality.
 * Two boards are equal iff the cell occupancy, the player to move, AND the active
 * tile match -- major_tile is part of the game state (it constrains the legal
 * moves), so it must be part of the transposition-table key.
 */
bool Board::operator==(const Board &other) const {
  return x == other.x && o == other.o && player == other.player && major_tile.i == other.major_tile.i &&
         major_tile.j == other.major_tile.j;
}

/*
 * Full 64-bit hash over exactly the fields compared by operator==
 * (occupancy, player, active tile). supergrid/winner are derived and excluded.
 */
uint64_t Board::hash() const {
  uint64_t h = fmix64((uint64_t)x ^ 0x9E3779B97F4A7C15ULL);
  h = fmix64(h ^ (uint64_t)(x >> 64));
  h = fmix64(h ^ (uint64_t)o);
  h = fmix64(h ^ (uint64_t)(o >> 64));
  uint64_t meta = (uint64_t)(uint8_t)player | ((uint64_t)(uint8_t)(major_tile.i + 1) << 8) |
                  ((uint64_t)(uint8_t)(major_tile.j + 1) << 16);
  h = fmix64(h ^ meta);
  return h;
}
