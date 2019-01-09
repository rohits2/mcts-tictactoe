var PLAYER_NONE = 0;
var PLAYER_X = -1;
var PLAYER_O = 1;
var PLAYER_TIE = 100;
function nextPlayer(player) {
    switch (player) {
        case PLAYER_X:
            return PLAYER_O;
        case PLAYER_O:
            return PLAYER_X;
        default:
            return PLAYER_NONE;
    }
}
function supergridWinner(grid) {
    var nonzeroCount = 0;
    for (var i = 0; i < 3; i++) {
        var rowSum = 0;
        var colSum = 0;
        for (var j = 0; j < 3; j++) {
            rowSum += grid[i * 3 + j];
            colSum += grid[j * 3 + i];
            nonzeroCount += grid[j * 3 + i] != PLAYER_NONE ? 1 : 0;
        }
        if (rowSum == 3 * PLAYER_X || colSum == 3 * PLAYER_X) {
            return PLAYER_X;
        }
        if (rowSum == 3 * PLAYER_O || colSum == 3 * PLAYER_O) {
            return PLAYER_O;
        }
    }
    var diagSum1 = grid[0 * 3 + 0] + grid[1 * 3 + 1] + grid[2 * 3 + 2];
    var diagSum2 = grid[0 * 3 + 2] + grid[1 * 3 + 1] + grid[2 * 3 + 0];
    if (diagSum1 == 3 * PLAYER_X || diagSum2 == 3 * PLAYER_X) {
        return PLAYER_X;
    }
    if (diagSum1 == 3 * PLAYER_O || diagSum2 == 3 * PLAYER_O) {
        return PLAYER_O;
    }
    if (nonzeroCount == 3 * 3) {
        return PLAYER_TIE;
    }
    return PLAYER_NONE;
}
function subgridWinner(grid, m_i, m_j) {
    var nonzeroCount = 0;
    for (var i = 0; i < 3; i++) {
        var rowSum = 0;
        var colSum = 0;
        for (var j = 0; j < 3; j++) {
            rowSum += grid[(3 * m_i + i) * 9 + j + 3 * m_j];
            colSum += grid[(3 * m_i + j) * 9 + i + 3 * m_j];
            nonzeroCount += grid[(3 * m_i + i) * 3 + j + 3 * m_j] ? 1 : 0;
        }
        if (rowSum == 3 * PLAYER_X || colSum == 3 * PLAYER_X) {
            return PLAYER_X;
        }
        if (rowSum == 3 * PLAYER_O || colSum == 3 * PLAYER_O) {
            return PLAYER_O;
        }
    }
    var diagSum1 = grid[(3 * m_i) * 9 + 3 * m_j] +
        grid[(3 * m_i + 1) * 9 + 3 * m_j + 1] +
        grid[(3 * m_i + 2) * 9 + 3 * m_j + 2];
    var diagSum2 = grid[(3 * m_i) * 9 + 3 * m_j + 2] +
        grid[(3 * m_i + 1) * 9 + 3 * m_j + 1] + grid[(3 * m_i + 2) * 9 + 3 * m_j];
    if (diagSum1 == 3 * PLAYER_X || diagSum2 == 3 * PLAYER_X) {
        return PLAYER_X;
    }
    if (diagSum1 == 3 * PLAYER_O || diagSum2 == 3 * PLAYER_O) {
        return PLAYER_O;
    }
    // printf("NZC %d\n", nonzero_count);
    if (nonzeroCount == 3 * 3) {
        return PLAYER_TIE;
    }
    return PLAYER_NONE;
}
var Board = /** @class */ (function () {
    function Board() {
        this.board = new Int8Array(9 * 9);
        this.supergrid = new Int8Array(9);
        this.player = PLAYER_X;
        this.majorTile = null;
    }
    Board.prototype.getCell = function (i, j, ii, jj) {
        var row = i * 3 + ii;
        var col = j * 3 + jj;
        return this.board[row * 9 + col];
    };
    Board.prototype.getSupergridCell = function (i, j) {
        return this.supergrid[i * 3 + j];
    };
    Board.prototype.isValidMove = function (i, j, ii, jj) {
        if (this.majorTile == null) {
            return this.getCell(i, j, ii, jj) == PLAYER_NONE;
        }
        var _a = this.majorTile, mI = _a[0], mJ = _a[1];
        return (mI == i && mJ == j) &&
            this.getCell(i, j, ii, jj) == PLAYER_NONE &&
            this.supergrid[i * 3 + j] == PLAYER_NONE;
    };
    Board.prototype.getValidMoves = function () {
        var moves = [];
        if (this.majorTile == null) {
            for (var i = 0; i < 3; i++) {
                for (var j = 0; j < 3; j++) {
                    for (var ii = 0; ii < 3; ii++) {
                        for (var jj = 0; jj < 3; jj++) {
                            if (this.isValidMove(i, j, ii, jj)) {
                                moves.push([i, j, ii, jj]);
                            }
                        }
                    }
                }
            }
        }
        else {
            var _a = this.majorTile, i = _a[0], j = _a[1];
            for (var ii = 0; ii < 3; ii++) {
                for (var jj = 0; jj < 3; jj++) {
                    if (this.isValidMove(i, j, ii, jj)) {
                        moves.push([i, j, ii, jj]);
                    }
                }
            }
        }
        return moves;
    };
    Board.prototype.move = function (i, j, ii, jj) {
        if (!this.isValidMove(i, j, ii, jj)) {
            return;
        }
        var row = i * 3 + ii;
        var col = j * 3 + jj;
        var ind = row * 9 + col;
        this.board[ind] = this.player;
        this.player = nextPlayer(this.player);
        this.supergrid[i * 3 + j] = subgridWinner(this.board, i, j);
        if (this.supergrid[ii * 3 + jj] != PLAYER_NONE) {
            this.majorTile = null;
        }
        else {
            this.majorTile = [ii, jj];
        }
    };
    Board.prototype.gameWinner = function () {
        return supergridWinner(this.supergrid);
    };
    return Board;
}());
