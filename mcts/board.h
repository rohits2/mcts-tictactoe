#ifndef BOARD_H
#define BOARD_H
#include <functional>
#include <iostream>
#include <tuple>
#include <vector>

using std::vector, std::tuple, std::get, std::cout, std::endl, std::hash, std::cout, std::endl;
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

class Board {
  public:
    Board(const Board &other);
    Board(const char grid[9][9], const int active_player, const supergrid_coord active_tile);
    Board();
    vector<grid_coord> get_valid_moves() const;
    char game_winner() const;
    bool is_valid_move(const grid_coord &move) const;
    bool move(const grid_coord &move);
    void print();
    bool operator==(const Board &other) const;
    char board[9][9] = {PLAYER_NONE};
    char supergrid[3][3] = {PLAYER_NONE};
    char player = PLAYER_X;
    supergrid_coord major_tile = {.i = -1, .j = -1};

  private:
    void update_supergrid(int i, int j);
    void update_supergrid();
};

inline bool operator==(const grid_coord &a, const grid_coord &b) { return a.m_i == b.m_i && a.m_j == b.m_j && a.i == b.i && a.j == b.j; }

namespace std {
template <> struct hash<grid_coord> {
    size_t operator()(const grid_coord &gc) const {
        //We know grid-coordinates will always be in [0, 9] so we can bitpack for an efficient hash.
        long long hash = gc.m_i << 24 | gc.m_j << 16 | gc.i << 8 | gc.j;
        return hash;
    }
};
template <> struct hash<Board> {
    size_t operator()(const Board &board) const {
        //There are 81 board cells, each of which encodes 2 bits.
        //We can therefore encode the entire board perfectly in 20.25 bytes.
        //We can XOR each size_t worth of bytes together for a fast hash.
        unsigned long long hash = 0;
        char *board_arr = (char *)board.board;
        for (int i = 0; i < 81; i++) {
            unsigned long long half_byte;
            switch (board_arr[i]) {
            case PLAYER_NONE:
                half_byte = 0;
                break;
            case PLAYER_X:
                half_byte = 1;
                break;
            case PLAYER_O:
                half_byte = 2;
                break;
            }
            half_byte <<= ((2 * i) % (sizeof(unsigned long long) * 4));
            hash ^= half_byte;
        }
        return hash;
    }
};
} // namespace std
#endif