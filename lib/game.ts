const PLAYER_NONE = 0;
const PLAYER_X = -1;
const PLAYER_O = 1;
const PLAYER_TIE = 100;

function nextPlayer(player: number): number {
  switch (player) {
    case PLAYER_X:
      return PLAYER_O;
    case PLAYER_O:
      return PLAYER_X;
    default:
      return PLAYER_NONE;
  }
}

function supergridWinner(grid: Int8Array) {
  let nonzeroCount = 0;
  for (let i = 0; i < 3; i++) {
    let rowSum = 0;
    let colSum = 0;
    for (let j = 0; j < 3; j++) {
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
  let diagSum1 = grid[0 * 3 + 0] + grid[1 * 3 + 1] + grid[2 * 3 + 2];
  let diagSum2 = grid[0 * 3 + 2] + grid[1 * 3 + 1] + grid[2 * 3 + 0];
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

function subgridWinner(grid: Int8Array, m_i: number, m_j: number) {
  let nonzeroCount = 0;
  for (let i = 0; i < 3; i++) {
    let rowSum = 0;
    let colSum = 0;
    for (let j = 0; j < 3; j++) {
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
  let diagSum1 = grid[(3 * m_i) * 9 + 3 * m_j] +
      grid[(3 * m_i + 1) * 9 + 3 * m_j + 1] +
      grid[(3 * m_i + 2) * 9 + 3 * m_j + 2];
  let diagSum2 = grid[(3 * m_i) * 9 + 3 * m_j + 2] +
      grid[(3 * m_i + 1) * 9 + 3 * m_j + 1] + grid[(3 * m_i + 2) * 9 + 3 * m_j];
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

class Board {
  board: Int8Array;
  supergrid: Int8Array;
  player: number;
  majorTile: [number, number]
  constructor() {
    this.board = new Int8Array(9 * 9);
    this.supergrid = new Int8Array(9);
    this.player = PLAYER_X;
    this.majorTile = null;
  }
  getCell(i, j, ii, jj) {
    let row = i * 3 + ii;
    let col = j * 3 + jj;
    return this.board[row * 9 + col];
  }
  getSupergridCell(i, j) {
    return this.supergrid[i * 3 + j];
  }
  isValidMove(i, j, ii, jj) {
    if (this.majorTile == null) {
      return this.getCell(i, j, ii, jj) == PLAYER_NONE && this.getSupergridCell(i, j) == PLAYER_NONE;
    }
    let [mI, mJ] = this.majorTile
    return (mI == i && mJ == j) &&
        this.getCell(i, j, ii, jj) == PLAYER_NONE &&
        this.supergrid[i * 3 + j] == PLAYER_NONE;
  }
  getValidMoves() {
    let moves = [];
    if (this.majorTile == null) {
      for (let i = 0; i < 3; i++) {
        for (let j = 0; j < 3; j++) {
          for (let ii = 0; ii < 3; ii++) {
            for (let jj = 0; jj < 3; jj++) {
              if (this.isValidMove(i, j, ii, jj)) {
                moves.push([i, j, ii, jj]);
              }
            }
          }
        }
      }
    } else {
      let [i, j] = this.majorTile;
      for (let ii = 0; ii < 3; ii++) {
        for (let jj = 0; jj < 3; jj++) {
          if (this.isValidMove(i, j, ii, jj)) {
            moves.push([i, j, ii, jj]);
          }
        }
      }
    }
    return moves;
  }
  move(i, j, ii, jj) {
    if (!this.isValidMove(i, j, ii, jj)) {
      return;
    }
    let row = i * 3 + ii;
    let col = j * 3 + jj;
    let ind = row * 9 + col;
    this.board[ind] = this.player;
    this.player = nextPlayer(this.player);
    this.supergrid[i * 3 + j] = subgridWinner(this.board, i, j);
    if (this.supergrid[ii * 3 + jj] != PLAYER_NONE) {
      this.majorTile = null;
    } else {
      this.majorTile = [ii, jj];
    }
  }
  gameWinner() {
    return supergridWinner(this.supergrid);
  }
}