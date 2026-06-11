#ifndef BOARD_H
#define BOARD_H
#include <cstdint>
#include <functional>
#include <iostream>
#include <tuple>
#include <vector>

typedef struct _grid_coord {
    int m_i;
    int m_j;
    int i;
    int j;
} grid_coord;

typedef struct _supergrid_coord {
    int i;
    int j;
} supergrid_coord;

const char PLAYER_NONE = 0;
const char PLAYER_X = -1;
const char PLAYER_O = 1;
const char PLAYER_TIE = 100;
const char PLAYER_UNSPECIFIED = 92;

// The 81 cells are stored as two bitboards (one per player) indexed subgrid-major:
//   bit(g_i, g_j) = subgrid * 9 + local
//   subgrid = (g_i / 3) * 3 + (g_j / 3)      local = (g_i % 3) * 3 + (g_j % 3)
// so each 3x3 subgrid is a contiguous 9-bit block. Win detection then reduces to
// masking a 9-bit field against the eight winning lines. This replaces the old
// char[9][9] (81 bytes) + char[3][3] layout: sizeof(Board) drops from 100 to 48.
typedef unsigned __int128 board_mask;

class Board {
  public:
    Board(const char grid[9][9], const int active_player, const supergrid_coord active_tile);
    Board();
    // Copy is the implicit member-wise (trivial) copy: every field is already a
    // valid cache, so there is nothing to recompute.

    std::vector<grid_coord> get_valid_moves() const;
    // Picks one uniformly-random legal move with no heap allocation (for rollouts).
    // Returns false only when there is no legal move (terminal position).
    bool random_move(grid_coord &out) const;
    char game_winner() const;
    bool is_valid_move(const grid_coord &move) const;
    bool move(const grid_coord &move);
    void print();
    bool operator==(const Board &other) const;
    uint64_t hash() const;

    // Cell value at board coordinates (g_i, g_j): PLAYER_NONE / PLAYER_X / PLAYER_O.
    char cell(int g_i, int g_j) const;

    board_mask x = 0;          // X occupancy bitboard (81 of 128 bits used)
    board_mask o = 0;          // O occupancy bitboard
    uint16_t sx = 0;           // supergrid: subgrids won by X (9 bits)
    uint16_t so = 0;           // supergrid: subgrids won by O (9 bits)
    uint16_t sclosed = 0;      // supergrid: subgrids that are won or tied, i.e. unplayable
    char winner = PLAYER_NONE; // cached overall winner
    char player = PLAYER_X;    // player to move
    supergrid_coord major_tile = {.i = -1, .j = -1};

  private:
    void place(int subgrid, int local, char who);
    void recompute_subgrid(int subgrid);
    void recompute_winner();
    void recompute_all();
};

inline bool operator==(const grid_coord &a, const grid_coord &b) { return a.m_i == b.m_i && a.m_j == b.m_j && a.i == b.i && a.j == b.j; }

namespace std {
template <> struct hash<grid_coord> {
    size_t operator()(const grid_coord &gc) const {
        // Grid coordinates are always in [0, 9), so we can bitpack for an efficient hash.
        long long hash = gc.m_i << 24 | gc.m_j << 16 | gc.i << 8 | gc.j;
        return hash;
    }
};
template <> struct hash<Board> {
    size_t operator()(const Board &board) const { return board.hash(); }
};
} // namespace std
#endif
