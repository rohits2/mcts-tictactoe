#include "board.h"

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

char grid_winner(const char grid[3][3]) {
    int nonzero_count = 0;
    for (int i = 0; i < 3; i++) {
        int row_sum = 0;
        int col_sum = 0;
        for (int j = 0; j < 3; j++) {
            row_sum += grid[i][j];
            col_sum += grid[j][i];
            nonzero_count += grid[j][i] != PLAYER_NONE ? 1 : 0;
        }
        if (row_sum == 3 * PLAYER_X || col_sum == 3 * PLAYER_X) {
            return PLAYER_X;
        }
        if (row_sum == 3 * PLAYER_O || col_sum == 3 * PLAYER_O) {
            return PLAYER_O;
        }
    }
    int diag_sum_1 = grid[0][0] + grid[1][1] + grid[2][2];
    int diag_sum_2 = grid[0][2] + grid[1][1] + grid[2][0];
    if (diag_sum_1 == 3 * PLAYER_X || diag_sum_2 == 3 * PLAYER_X) {
        return PLAYER_X;
    }
    if (diag_sum_1 == 3 * PLAYER_O || diag_sum_2 == 3 * PLAYER_O) {
        return PLAYER_O;
    }
    if (nonzero_count == 3 * 3) {
        return PLAYER_TIE;
    }
    return PLAYER_NONE;
}

char grid_winner(const char grid[9][9], int m_i, int m_j) {
    int nonzero_count = 0;
    for (int i = 0; i < 3; i++) {
        int row_sum = 0;
        int col_sum = 0;
        for (int j = 0; j < 3; j++) {
            row_sum += grid[3 * m_i + i][j + 3 * m_j];
            col_sum += grid[3 * m_i + j][i + 3 * m_j];
            nonzero_count += grid[3 * m_i + i][j + 3 * m_j] ? 1 : 0;
        }
        if (row_sum == 3 * PLAYER_X || col_sum == 3 * PLAYER_X) {
            return PLAYER_X;
        }
        if (row_sum == 3 * PLAYER_O || col_sum == 3 * PLAYER_O) {
            return PLAYER_O;
        }
    }
    int diag_sum_1 = grid[3 * m_i][3 * m_j] + grid[3 * m_i + 1][3 * m_j + 1] + grid[3 * m_i + 2][3 * m_j + 2];
    int diag_sum_2 = grid[3 * m_i][3 * m_j + 2] + grid[3 * m_i + 1][3 * m_j + 1] + grid[3 * m_i + 2][3 * m_j];
    if (diag_sum_1 == 3 * PLAYER_X || diag_sum_2 == 3 * PLAYER_X) {
        return PLAYER_X;
    }
    if (diag_sum_1 == 3 * PLAYER_O || diag_sum_2 == 3 * PLAYER_O) {
        return PLAYER_O;
    }
    // printf("NZC %d\n", nonzero_count);
    if (nonzero_count == 3 * 3) {
        return PLAYER_TIE;
    }
    return PLAYER_NONE;
}

bool is_unset(supergrid_coord tile) { return (tile.i == -1) && (tile.j == -1); }

Board::Board() {}

Board::Board(const Board &other) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            board[i][j] = other.board[i][j];
        }
    }
    player = other.player;
    major_tile = other.major_tile;
    update_supergrid();
}

Board::Board(const char grid[9][9], const int active_player, const supergrid_coord active_tile) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            board[i][j] = grid[i][j];
        }
    }
    player = active_player;
    major_tile = active_tile;
    update_supergrid();
}

char Board::game_winner() const { return grid_winner(supergrid); }

vector<grid_coord> Board::get_valid_moves() const {
    if (game_winner() == PLAYER_TIE) {
        return vector<grid_coord>();
    }
    vector<grid_coord> moves;
    if (is_unset(major_tile)) {
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 9; j++) {
                if(supergrid[i/3][j/3] != PLAYER_NONE){
                    continue;
                }
                if (board[i][j] == PLAYER_NONE) {
                    moves.push_back(grid_coord{.m_i = i / 3, .m_j = j / 3, .i = i % 3, .j = j % 3});
                }
            }
        }
    } else {
        for (int i = major_tile.i * 3; i < major_tile.i * 3 + 3; i++) {
            for (int j = major_tile.j * 3; j < major_tile.j * 3 + 3; j++) {
                if (board[i][j] == PLAYER_NONE) {
                    moves.push_back(grid_coord{.m_i = i / 3, .m_j = j / 3, .i = i % 3, .j = j % 3});
                }
            }
        }
    }
    return moves;
}

bool Board::is_valid_move(const grid_coord &move) const {
    int i = move.i;
    int j = move.j;
    int m_i = move.m_i;
    int m_j = move.m_j;
    if (is_unset(major_tile)) {
        return board[i + 3 * m_i][j + 3 * m_j] == PLAYER_NONE;
    }
    return board[i + 3 * m_i][j + 3 * m_j] == PLAYER_NONE && (major_tile.i == m_i || major_tile.j == m_j);
}

bool Board::move(const grid_coord &move) {
    int i = move.i;
    int j = move.j;
    int m_i = move.m_i;
    int m_j = move.m_j;
    if (!is_valid_move(move)) {
        return false;
    }
    int g_i = m_i * 3 + i;
    int g_j = m_j * 3 + j;
    board[g_i][g_j] = player;
    update_supergrid(m_i, m_j);
    if (supergrid[i][j] != PLAYER_NONE) {
        major_tile = {.i = -1, .j = -1};
    } else {
        major_tile = {.i = i, .j = j};
    }
    player = get_next_player(player);
    return true;
}

void Board::update_supergrid(int m_i, int m_j) { supergrid[m_i][m_j] = grid_winner(board, m_i, m_j); }

void Board::update_supergrid() {
    for (int m_i = 0; m_i < 3; m_i++) {
        for (int m_j = 0; m_j < 3; m_j++) {
            supergrid[m_i][m_j] = grid_winner(board, m_i, m_j);
        }
    }
}

void Board::print() {
    cout << endl;
    for (int i = 0; i < 9; i++) {
        if (i % 3 == 0 && i > 0) {
            cout << "-----------" << endl;
        }
        for (int j = 0; j < 9; j++) {
            if (j % 3 == 0 && j > 0) {
                cout << "|";
            }
            switch (board[i][j]) {
            case PLAYER_X:
                cout << "X";
                break;
            case PLAYER_O:
                cout << "O";
                break;
            case PLAYER_NONE:
                cout << " ";
                break;
            default:
                cout << "ERROR: INVALID VALUE IN BOARD!" << endl;
                break;
            }
        }
        cout << endl;
    }
    for (int i = 0; i < 3; i++) {
        if (i > 0) {
            cout << "-----" << endl;
        }
        for (int j = 0; j < 3; j++) {
            if (j > 0) {
                cout << "|";
            }
            switch (supergrid[i][j]) {
            case PLAYER_X:
                cout << "X";
                break;
            case PLAYER_O:
                cout << "O";
                break;
            case PLAYER_NONE:
                cout << " ";
                break;
            default:
                cout << "ERROR: INVALID VALUE IN BOARD!" << endl;
                break;
            }
        }
        cout << endl;
    }
    cout << endl;
}

bool Board::operator==(const Board &other) const {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            if (board[i][j] != other.board[i][j]) {
                return false;
            }
        }
    }
    return player == other.player;
}