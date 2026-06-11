var getMove = cwrap('get_move', 'number', ['array', 'number', 'number', 'number']);
var getTranspositionSize = cwrap('transposition_table_size', 'number', []);
var getValue = cwrap('get_value', 'number', ['array', 'number', 'number', 'number']);
var getWinProb = cwrap('get_win_prob', 'number', ['array', 'number', 'number', 'number']);
var getTieProb = cwrap('get_tie_prob', 'number', ['array', 'number', 'number', 'number']);
// Non-blocking engine API: keep the background search pointed at the live
// position (ponder), then poll it for a move instead of blocking the UI thread.
var ponderBoard = cwrap('ponder', null, ['array', 'number', 'number', 'number']);
var peekMove = cwrap('peek_move', 'number', ['array', 'number', 'number', 'number']);
var nodeVisits = cwrap('node_visits', 'number', ['array', 'number', 'number', 'number']);
var rolloutCount = cwrap('rollouts', 'number', []);
var heapBytes = cwrap('heap_bytes', 'number', []);
var isReady = cwrap('is_ready', 'number', []);

// Let the background search add this many FRESH rollouts to the live position
// before the engine commits a move, or the time budget elapses -- whichever comes
// first. (Fresh, not cumulative, so a deeply-pondered position still gets real
// search rather than committing instantly.)
var VISIT_TARGET = 100000;
var TIME_BUDGET_MS = 4000;

var board;
var gridDiv;
var chart;
var xScores = [];
var oScores = [];
var runtimeReady = false; // set only once the WASM worker actually exists
var moveOk = true;
var statsTimer = null;
var prevRollouts = 0;
var prevStatsTime = 0;
var engineGen = 0; // bumped to cancel any in-flight engine-move poll loop
var showHelp = false; // overlay per-move win probabilities ("Show Help")


/**
 * Set up the board and SVG holders.
 */
function init() {
  engineGen++; // cancel any engine-move poll still running from a previous game
  board = new Board();
  gridDiv = document.getElementById('svg-holder');
  player = PLAYER_X;
  xScores = [];
  oScores = [];
  moveOk = true;
  document.getElementById('loading').style.visibility = 'hidden';
  updateScoreReadout();
  draw();
  ponderCurrent(); // start chewing on the opening position right away
}
/**
 * Set up the board and SVG holders, and request a move for X from the AI.
 */
function initO() {
  engineGen++; // cancel any engine-move poll still running from a previous game
  board = new Board();
  xScores = [];
  oScores = [];
  updateScoreReadout();
  gridDiv = document.getElementById('svg-holder');
  moveOk = false;
  document.getElementById('loading').style.visibility = 'visible';
  // The AI (X) opens, computed in the background; commitEngineMove draws the
  // board and then ponders the human's (O's) reply.
  requestEngineMove();
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
  requestEngineMove();
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
  drawHints(grid);
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
  // Pull the win and tie rates for the player to move straight from the search
  // tree's visit counts, then map them onto X and O. drawScores only runs on
  // the player-to-move's turn, but win + tie + loss covers all three outcomes,
  // so the opponent's win probability is just the remainder. (get_value returns
  // a single blended scalar that can't be split back into win vs tie.)
  //
  // Do NOT call get_move here just to prime data: it now blocks until
  // MIN_VISITS_PER_MOVE rollouts, and blocking on the browser's main thread
  // stalls the whole render so the chart never draws. The background worker
  // already searches the live position, so just read it; fall back to 50/50 if
  // a read fails so the chart still renders.
  let moverWin = 0.5, moverTie = 0;
  try {
    moverWin = getWinProb(board.board, board.player, oI, oJ);
    moverTie = getTieProb(board.board, board.player, oI, oJ);
  } catch (e) {
    console.error('drawScores: win/tie probability read failed', e);
  }
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

  try {
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
  } catch (e) {
    console.error('drawScores: chart render failed', e);
  }
}

/**
 * The active tile (forced subgrid) as [i, j], or [-1, -1] for a free move.
 */
function activeTile() {
  return board.majorTile != null ? board.majorTile : [-1, -1];
}

/**
 * Keep the background worker searching (pondering) the live position, including
 * while it's the human's turn -- so when they move, the engine has a head start.
 */
function ponderCurrent() {
  if (!runtimeReady || !board) return;
  if (board.gameWinner() != PLAYER_NONE) return;
  let [oI, oJ] = activeTile();
  try { ponderBoard(board.board, board.player, oI, oJ); } catch (e) {}
}

/**
 * Ask the engine to move WITHOUT blocking the UI: point the background search at
 * the current position, then poll it each frame until it has searched enough (or
 * the time budget elapses), and play the best move found.
 */
function requestEngineMove(retries) {
  retries = retries || 0;
  if (!runtimeReady) {
    // Worker not constructed yet; wait for it rather than calling a null engine.
    if (retries < 100) {
      setTimeout(function () { requestEngineMove(retries + 1); }, 100);
    } else {
      finishTurn(); // give up gracefully after ~10s so the UI isn't stuck
    }
    return;
  }
  if (board.gameWinner() != PLAYER_NONE) {
    drawScores();
    finishTurn();
    return;
  }
  let myGen = ++engineGen; // invalidates any older in-flight poll loop
  let [oI, oJ] = activeTile();
  try { ponderBoard(board.board, board.player, oI, oJ); } catch (e) {}
  let base = 0;
  try { base = nodeVisits(board.board, board.player, oI, oJ); } catch (e) {}
  let start = performance.now();
  (function poll() {
    if (myGen !== engineGen) return; // a restart / newer request superseded us
    let visits = base;
    try { visits = nodeVisits(board.board, board.player, oI, oJ); } catch (e) {}
    if ((visits - base) >= VISIT_TARGET || (performance.now() - start) >= TIME_BUDGET_MS) {
      commitEngineMove(myGen);
    } else {
      requestAnimationFrame(poll);
    }
  })();
}

/**
 * Play the best move the background search found; fall back to a known-valid move
 * if the engine hands back something unplayable (e.g. the free-choice case).
 */
function commitEngineMove(myGen) {
  if (myGen !== engineGen) return; // superseded by a newer request / restart
  let [oI, oJ] = activeTile(); // re-derive from the live board, never a stale capture
  let iMove = -1;
  try { iMove = peekMove(board.board, board.player, oI, oJ); } catch (e) { iMove = -1; }
  let cI = iMove >> 24 & 0xFF, cJ = iMove >> 16 & 0xFF, cII = iMove >> 8 & 0xFF, cJJ = iMove & 0xFF;
  let played = board.move(cI, cJ, cII, cJJ);
  if (!played) {
    // Unsearched / unplayable best move: play a known-valid move so the game continues.
    let validMoves = board.getValidMoves();
    if (validMoves.length) {
      let mv = validMoves[0];
      board.move(mv[0], mv[1], mv[2], mv[3]);
    }
  }
  drawScores();
  finishTurn();
  ponderCurrent(); // immediately start pondering the human's new position
}

/**
 * Redraw, hide the spinner, and re-enable input.
 */
function finishTurn() {
  draw();
  document.getElementById('loading').style.visibility = 'hidden';
  moveOk = true;
}

/**
 * Format a count compactly (1234 -> 1k, 1200000 -> 1.2M).
 */
function fmtCount(n) {
  if (!Number.isFinite(n)) return '0';
  if (n >= 1e6) return (n / 1e6).toFixed(1) + 'M';
  if (n >= 1e3) return (n / 1e3).toFixed(0) + 'k';
  return '' + Math.round(n);
}

/**
 * Realtime tree stats below the win probabilities: live node count, search
 * throughput (rollouts/sec), and engine heap size.
 */
function updateTreeStats() {
  let el = document.getElementById('tree-stats');
  if (!el || !runtimeReady) return;
  try {
    let nodes = getTranspositionSize();
    let roll = rolloutCount();
    let now = performance.now();
    if (prevStatsTime == 0) { prevStatsTime = now; prevRollouts = roll; }
    let dt = (now - prevStatsTime) / 1000;
    let rate = dt > 0 ? Math.max(0, (roll - prevRollouts) / dt) : 0;
    prevStatsTime = now;
    prevRollouts = roll;
    let mem = 0;
    try { mem = heapBytes(); } catch (e) {}
    let memMB = Number.isFinite(mem) ? (mem / 1048576).toFixed(0) : '--';
    el.innerHTML =
      '<table>' +
      '<tr><td class="ts-key">nodes</td><td class="ts-val">' + fmtCount(nodes) + '</td></tr>' +
      '<tr><td class="ts-key">rollout/s</td><td class="ts-val">' + fmtCount(rate) + '</td></tr>' +
      '<tr><td class="ts-key">mem</td><td class="ts-val">' + memMB + ' MB</td></tr>' +
      '</table>';
    // Keep the per-move hints fresh while Help is on and it's the human's turn.
    if (showHelp && moveOk && board && board.gameWinner() == PLAYER_NONE) {
      draw();
    }
  } catch (e) {}
}

/**
 * Mark the engine ready (the worker exists) and start the perpetual stats loop.
 */
function onReady() {
  if (runtimeReady) return;
  runtimeReady = true;
  prevStatsTime = 0;
  if (!statsTimer) statsTimer = setInterval(updateTreeStats, 400);
  ponderCurrent();
}

// Poll is_ready() until main() has constructed the worker, THEN start pondering
// and stats. is_ready() touches no engine state (it just returns whether the
// worker exists), so it's safe to call before the worker is built -- avoiding the
// PROXY_TO_PTHREAD trap where onRuntimeInitialized can fire before main() finishes.
(function () {
  function pollReady() {
    if (runtimeReady) return;
    let ready = 0;
    try { ready = isReady(); } catch (e) { ready = 0; }
    if (ready) { onReady(); } else { setTimeout(pollReady, 60); }
  }
  if (typeof Module !== 'undefined' && !Module.calledRun) {
    var prev = Module.onRuntimeInitialized;
    Module.onRuntimeInitialized = function () {
      if (typeof prev === 'function') prev();
      pollReady();
    };
  }
  setTimeout(pollReady, 300); // also covers calledRun==true and a missed hook
})();

/**
 * Make an independent copy of a board so we can try a move on it without
 * disturbing the live game.
 */
function cloneBoard(b) {
  let c = new Board();
  c.board = b.board.slice();
  c.supergrid = b.supergrid.slice();
  c.player = b.player;
  c.majorTile = b.majorTile != null ? [b.majorTile[0], b.majorTile[1]] : null;
  return c;
}

/**
 * When Help is on, overlay each valid move with the win probability for the
 * player to move if they play there, read from the background search.
 */
function drawHints(grid) {
  if (!showHelp || !runtimeReady || !board) return;
  if (board.gameWinner() != PLAYER_NONE) return;
  board.getValidMoves().forEach(move => {
    let [i, j, ii, jj] = move;
    let child = cloneBoard(board);
    child.move(i, j, ii, jj);
    let [cI, cJ] = child.majorTile != null ? child.majorTile : [-1, -1];
    let p = 0;
    try {
      // child.player is the OPPONENT (to move after our move), so the
      // mover-here win probability is the remainder after their win + tie.
      let oppWin = getWinProb(child.board, child.player, cI, cJ);
      let oppTie = getTieProb(child.board, child.player, cI, cJ);
      p = Math.max(0, Math.min(1, 1 - oppWin - oppTie));
    } catch (e) {}
    grid.drawHint(i, j, ii, jj, Math.round(p * 100) + '%');
  });
}

/**
 * Toggle the per-move win-probability hints (the "Show Help" button).
 */
function toggleHints() {
  showHelp = !showHelp;
  let btn = document.getElementById('hint-button');
  if (btn) btn.innerText = showHelp ? 'Hide Help' : 'Show Help';
  if (board && moveOk) draw(); // redraw immediately if it's the human's turn
}