var getMove = cwrap('get_move', 'number', ['array', 'number', 'number', 'number']);
var getTranspositionSize = cwrap('transposition_table_size', 'number', []);
var getValue = cwrap('get_value', 'number', ['array', 'number', 'number', 'number']);
var getWinProb = cwrap('get_win_prob', 'number', ['array', 'number', 'number', 'number']);
var getTieProb = cwrap('get_tie_prob', 'number', ['array', 'number', 'number', 'number']);
var board;
var gridDiv;
var chart;
var xScores = [];
var oScores = [];
var mctsInitialized = false;
var moveOk = true;


/**
 * Set up the board and SVG holders.
 */
function init() {
  board = new Board();
  gridDiv = document.getElementById('svg-holder');
  player = PLAYER_X;
  xScores = [];
  oScores = [];
  updateScoreReadout();
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

  xScores = [];
  oScores = [];
  updateScoreReadout();

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
      if(!moveOk) return;
      moveOk = false;
      document.getElementById('loading').style.visibility = 'visible';
      drawScores();
      board.move(i, j, ii, jj);
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
              () => { computerMove(grid) }, 50));
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
  // Ask the engine for a move, but never spin forever. If it keeps handing back
  // an unplayable move (e.g. the free-choice case, where you're sent to an
  // already-won supercell), fall back to a known-valid move so the game can't
  // lock up.
  let played = false;
  for (let attempt = 0; attempt < 8 && !played; attempt++) {
    var iMove = getMove(board.board, board.player, oI, oJ);
    [cI, cJ, cII, cJJ] =
      [iMove >> 24 & 0xFF, iMove >> 16 & 0xFF, iMove >> 8 & 0xFF, iMove & 0xFF];
    played = board.move(cI, cJ, cII, cJJ);
  }
  if (!played) {
    let validMoves = board.getValidMoves();
    if (validMoves.length) {
      [cI, cJ, cII, cJJ] = validMoves[0];
      board.move(cI, cJ, cII, cJJ);
    }
  }

  drawScores();

  draw();
  document.getElementById('loading').style.visibility = 'hidden';
  moveOk = true;
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

function range(size, startAt = 0) {
  return [...Array(size).keys()].map(i => i + startAt);
}

/**
 * Update the numeric current X/O win-probability readout below the chart.
 */
function updateScoreReadout() {
  let el = document.getElementById('score-readout');
  if (!el) return;
  if (!xScores.length || !oScores.length) {
    el.innerHTML = "No data yet!";
    return;
  }
  let pct = v => (100 * v).toFixed(1) + '%';
  let x = xScores[xScores.length - 1];
  let o = oScores[oScores.length - 1];
  let tie = Math.max(0, 1 - x - o);
  el.innerHTML =
    '<span class="readout-x">X: ' + pct(x) + '</span>' +
    '<span class="readout-o">O: ' + pct(o) + '</span>' +
    '<span class="readout-tie">Tie: ' + pct(tie) + '</span>';
}

function drawScores() {
  let scoreDiv = document.getElementById('score-chart');
  scoreDiv.innerHTML = "";


  let [oI, oJ] = [-1, -1];
  if (board.majorTile != null) {
    [oI, oJ] = board.majorTile;
  }
  // Ensure that the MCTS engine has some data so that the first x-score is not 0.
  if(!mctsInitialized){
    mctsInitialized = true;
    getMove(board.board, board.player, oI, oJ);
  }

  // Pull the win and tie rates for the player to move straight from the search
  // tree's visit counts, then map them onto X and O. drawScores only runs on
  // the player-to-move's turn, but win + tie + loss covers all three outcomes,
  // so the opponent's win probability is just the remainder. (get_value returns
  // a single blended scalar that can't be split back into win vs tie.)
  let moverWin = getWinProb(board.board, board.player, oI, oJ);
  let moverTie = getTieProb(board.board, board.player, oI, oJ);
  let oppWin = Math.max(0, 1 - moverWin - moverTie);
  let xVal, oVal;
  if (board.player == PLAYER_X) {
    xVal = moverWin;
    oVal = oppWin;
  } else {
    oVal = moverWin;
    xVal = oppWin;
  }
  xScores.push(xVal);
  oScores.push(oVal);

  if (board.gameWinner() != 0) {
    if (board.gameWinner() == -1) {         // X wins
      xScores.push(1); oScores.push(0);
    } else if (board.gameWinner() == 1) {   // O wins
      xScores.push(0); oScores.push(1);
    } else {                                // draw
      xScores.push(0.5); oScores.push(0.5);
    }
  }

  updateScoreReadout();

  chart = new Chartist.Line('.score-chart', {
    labels: range(xScores.length, 1),
    series: [
      {
        className: "series-x",
        name: "X",
        data: xScores
      },
      {
        className: "series-o",
        name: "O",
        data: oScores
      }
    ]
  }, {
      height: "200px",
      width: "auto",
      chartPadding: {
        right: 10
      },
      lineSmooth: Chartist.Interpolation.cardinal({
        fillHoles: true,
      }),
      axisX: {
        showLabel: false,
        offset: 0
      },
      axisY: {
        type: Chartist.FixedScaleAxis,
        low: 0,
        high: 1.25,
        divisor: 5
      }
    });
}