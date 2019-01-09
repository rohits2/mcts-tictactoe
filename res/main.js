var getMove =
    cwrap('get_move', 'number', ['array', 'number', 'number', 'number']);
var getTranspositionSize = cwrap('transposition_table_size', 'number', []);
var board;
var gridDiv;
/**
 * Set up the board and SVG holders.
 */
function init() {
  board = new Board();
  gridDiv = document.getElementById('svg-holder');
  draw();
}
/**
 * Set up the board and SVG holders, and request a move for X from the AI.
 */
function initO() {
  board = new Board();
  var iMove = getMove(board.board, board.player, -1, -1);
  let [cI, cJ, cII, cJJ] = [
    iMove >> 24 & 0xFF, iMove >> 16 & 0xFF, iMove >> 8 & 0xFF, iMove & 0xFF
  ];  // Ugly bitpacking hack because I am so sick of JS
  board.move(cI, cJ, cII, cJJ);
  gridDiv = document.getElementById('svg-holder');
  draw();
}

/**
 * Given the SVG grid object, draw every board cell.
 * @param {SVG} grid
 */
function drawPositions(grid) {
  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      for (let ii = 0; ii < 3; ii++) {
        for (let jj = 0; jj < 3; jj++) {
          switch (board.getCell(i, j, ii, jj)) {
            case PLAYER_X:
              grid.drawSmallX(i, j, ii, jj);
              break;
            case PLAYER_O:
              grid.drawSmallO(i, j, ii, jj);
          }
        }
      }
    }
  }
  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      switch (board.getSupergridCell(i, j)) {
        case PLAYER_X:
          grid.drawBigX(i, j);
          break;
        case PLAYER_O:
          grid.drawBigO(i, j);
          break;
      }
    }
  }
}
/**
 * Given the SVG object, draw the clickable objects that will receive user
 * clicks.
 * @param {SVG} grid
 */
function drawMoves(grid) {
  board.getValidMoves().forEach(move => {
    let [i, j, ii, jj] = move;
    grid.addSmallLink(i, j, ii, jj, i + ' ' + j + ' ' + ii + ' ' + jj);
  });
}
/**
 * Given the SVG object, generate lambdas to connect the clickables with the
 * rest of the game script.
 * @param {SVG} grid
 */
function linkMoves(grid) {
  board.getValidMoves().forEach(move => {
    let [i, j, ii, jj] = move;
    let box = document.getElementById(i + ' ' + j + ' ' + ii + ' ' + jj);
    box.addEventListener('click', () => {
      board.move(i, j, ii, jj);
      document.getElementById('loading').style.visibility = 'visible';
      var grid = new Grid();
      drawPositions(grid);
      gridDiv.innerHTML = grid.render();
      // First, the loading icon is enabled and the grid is redrawn to include
      // the user move.
      // The actual move is computed in a callback, to give the display a chance
      // to update.
      window.requestAnimationFrame(
          () =>
              window
                  .requestAnimationFrame(  // Allow at least one frame to elapse
                                           // between the animation call
                      () => {computerMove(grid)}, 16));
    });
  });
}
/**
 * Allow the computer to make a move.
 * Calls out to WASM code using some convoluted bitpacking (I'm so sick of JS).
 * Also updates the display.
 * @param {SVG} grid
 */
function computerMove(grid) {
  let [oI, oJ] = [-1, -1];
  if (board.majorTile != null) {
    [oI, oJ] = board.majorTile;
  }
  var iMove = getMove(board.board, board.player, oI, oJ);
  [cI, cJ, cII, cJJ] =
      [iMove >> 24 & 0xFF, iMove >> 16 & 0xFF, iMove >> 8 & 0xFF, iMove & 0xFF];
  board.move(cI, cJ, cII, cJJ);
  draw();
  document.getElementById('loading').style.visibility = 'hidden';
}
/**
 * Draw and link the grid.
 */
function draw() {
  var grid = new Grid();
  drawPositions(grid);
  if (board.gameWinner() != PLAYER_NONE) {
    gridDiv.innerHTML = grid.render();
    return;
  }
  drawMoves(grid);
  gridDiv.innerHTML = grid.render();
  linkMoves(grid);
}