let SQRT3 = Math.sqrt(3);
let TAU = 2 * Math.PI;
let SCALE = 40;
let SIZE = 8;
let FIELDS = SIZE*(SIZE + 1)/2;  // SIZE = 8 -> FIELDS = 36
let INITIAL_STONES = 5;
let MAX_VALUE = 15;

let BASE36DIGITS = '0123456789abcdefghijklmnopqrstuvwxyz';

let RED_FIELD_STROKE = '#a01010';
let RED_FIELD_FILL = '#ff2020';
let RED_LABEL_FILL = 'black';
let BLUE_FIELD_STROKE = '#000080';
let BLUE_FIELD_FILL = '#0000a0';
let BLUE_LABEL_FILL = 'white';
let BROWN_FIELD_STROKE = '#301000';
let BROWN_FIELD_FILL = '#402000';
let EMPTY_LABEL_FILL = '#a0a0a0';
let EMPTY_FIELD_STROKE = '#202020';
let EMPTY_FIELD_FILL = '#e0e0e0';
let EMPTY_RED_LABEL_FILL = '#ffa0a0';
let EMPTY_BLUE_LABEL_FILL = '#a0a0ff';

var globalState = null;
var globalSelectedPiece = 0;

function moveIndexToPlayer(i) {
  if (i < INITIAL_STONES) return 0;
  if (i >= INITIAL_STONES + 2*MAX_VALUE) return null;
  return (i - INITIAL_STONES)%2 ? -1 : +1;
}

// Decodes and fully validates an encoded state string. (See encoding.txt for details.)
function decodeState(s) {
  if (s.length%2 != 0) {
    return null;
  }
  var fields = [];
  for (let i = 0; i < FIELDS; ++i) {
    fields[i] = null;
  }
  var pieces = {};
  for (var value = 1; value <= MAX_VALUE; ++value) {
    pieces[value] = pieces[-value] = true;
  }
  var history = [];
  var lastMove = null;
  for (let i = 0; i < s.length/2; ++i) {
    var fieldIndex = BASE36DIGITS.indexOf(s.charAt(2*i + 0));
    var stoneValue = BASE36DIGITS.indexOf(s.charAt(2*i + 1));
    if (fieldIndex < 0 || fieldIndex >= fields.length ||
        stoneValue < 0 || stoneValue > 2*MAX_VALUE) {
      return null;
    }
    if (stoneValue > MAX_VALUE) {
      stoneValue = MAX_VALUE - stoneValue;
    }
    if (fields[fieldIndex] !== null ||
        Math.sign(stoneValue) !== moveIndexToPlayer(i) ||
        (stoneValue != 0 && !pieces[stoneValue])) {
      return null;
    }
    fields[fieldIndex] = stoneValue;
    history.push({'fieldIndex': fieldIndex, 'stoneValue': stoneValue});
    if (stoneValue != 0) {
      lastMove = fieldIndex;
      pieces[stoneValue] = false;
    }
  }
  return {
    history: history,
    pieces: pieces,
    fields: fields,
    nextPlayer: moveIndexToPlayer(history.length),
    lastMove: lastMove,
  };
}

// Returns a new game state, or null if the move is invalid.
function doMove(state, u, v, pieceValue) {
  // We can reuse most of the validation logic in decodeState(). We just have to check
  // the input values are valid before encoding the move.
  if (!validCoords(u, v) || pieceValue < -MAX_VALUE || pieceValue > MAX_VALUE) {
    return null;
  }
  return decodeState(encodeState(state) + encodeMove(coordsToIndex(u, v), pieceValue));
}

function encodeMove(fieldIndex, stoneValue) {
  if (stoneValue < 0) stoneValue = MAX_VALUE - stoneValue;
  return BASE36DIGITS.charAt(fieldIndex) + BASE36DIGITS.charAt(stoneValue);
}

function encodeState(state) {
  let s = '';
  for (let move of state.history) {
    s += encodeMove(move.fieldIndex, move.stoneValue);
  }
  return s;
}

function boardCoords(u, v) {
  return {x: SCALE*(SIZE + v - u), y: SCALE*(1 + SQRT3*(u + v))};
}

function validCoords(u, v) {
  return 0 <= u && u < SIZE && 0 <= v && u + v < SIZE;
}

function coordsToIndex(u, v) {
  return SIZE*u - u*(u - 1)/2 + v;
}

function coordsToString(u, v) {
  return String.fromCharCode("A".charCodeAt(0) + u) + (v + 1);
}

function fieldIndexToCoords(i) {
  let u = 0, v = i, n = SIZE;
  while (v >= n) {
    v -= n;
    u += 1;
    n -= 1;
  }
  return {u: u, v: v};
}

function fieldIndexToString(i) {
  let coords = fieldIndexToCoords(i);
  return coordsToString(coords.u, coords.v);
}

function moveToString(move) {
  return fieldIndexToString(move.fieldIndex) + '=' + Math.abs(move.stoneValue);
}

function neighbours(u, v) {
  let neighbours = [];
  for (let du = -1; du <= 1; ++du) {
    for (let dv = -1; dv <= 1; ++dv) {
      if ((du != 0 || dv != 0) && Math.abs(du + dv) <= 1 && validCoords(u + du, v + dv)) {
        neighbours.push({u: u + du, v: v + dv});
      }
    }
  }
  return neighbours;
}

function calculateFieldInfluence(state, u, v) {
  let red = 0, blue = 0;
  for (let neighbour of neighbours(u, v)) {
    let v = state.fields[coordsToIndex(neighbour.u, neighbour.v)];
    if (v > 0) red += v;
    if (v < 0) blue -= v;
  }
  return [red, blue];
}

function calculateScore(state) {
  var delta = 0;
  for (let u = 0; u < SIZE; ++u) {
    for (let v = 0; u + v < SIZE; ++v) {
      if (state.fields[coordsToIndex(u, v)] === null) {
        let scores = calculateFieldInfluence(state, u, v);
        delta += scores[0] - scores[1];
      }
    }
  }
  return delta;
}

function setSelectedPiece(value) {
  globalSelectedPiece = Math.sign(value) == globalState.nextPlayer && globalState.pieces[value] ? value : 0;
  drawPieces(piecesCanvas, globalState, globalSelectedPiece);
}

function drawPieces(canvas, state, selectedPiece) {
  let context = canvas.getContext('2d');
  context.clearRect(0, 0, canvas.width, canvas.height);

  for (var r = 0; r < 6; ++r) {
    for (var c = 0; c < 5; ++c) {
      let value = 1 + 5*r + c;
      if (value > MAX_VALUE) value = MAX_VALUE - value;
      let selected = (value == selectedPiece);
      let fieldStroke = null;
      let fieldFill = null;
      let labelFill = null;
      let labelText = null;
      if (!state.pieces[value]) {
        fieldStroke = '#808080';
        fieldFill = '#e0e0e0';
        labelFill = '#808080';
      } else if (value > 0) {
        fieldStroke = RED_FIELD_STROKE;
        fieldFill = RED_FIELD_FILL;
        labelFill = RED_LABEL_FILL;
      } else {
        fieldStroke = BLUE_FIELD_STROKE;
        fieldFill = BLUE_FIELD_FILL;
        labelFill = BLUE_LABEL_FILL;
      }
      let cx = (2*c + 1)*SCALE*.8 + SCALE*.2;
      let cy = (2*r + 1)*SCALE*.8 + SCALE*.2;
      context.beginPath();
      context.lineWidth = selected ? 6 : 3;
      context.arc(cx, cy, SCALE*(selected ? .9 : .7) - 4, 0, TAU);
      context.fillStyle = fieldFill;
      context.fill();
      context.strokeStyle = fieldStroke;
      context.stroke();
      context.font = (selected ? '36px' : '32px') + ' sans-serif';
      context.textAlign = 'center';
      context.textBaseline = 'middle';
      context.fillStyle = labelFill;
      context.fillText(Math.abs(value), cx, cy);
    }
  }
}

function drawBoard(canvas, state) {
  let context = canvas.getContext('2d');
  context.clearRect(0, 0, canvas.width, canvas.height);

  context.beginPath();
  context.moveTo(SCALE*SIZE, SCALE);
  context.lineTo(SCALE*(2*SIZE - 1), SCALE*(1 + SQRT3*(SIZE - 1)));
  context.lineTo(SCALE, SCALE*(1 + SQRT3*(SIZE - 1)));
  context.closePath();
  context.fillStyle = EMPTY_FIELD_STROKE;
  context.fill();

  for (let u = 0; u < SIZE; ++u) {
    for (let v = 0; u + v < SIZE; ++v) {
      let fieldStroke = null;
      let fieldFill = null;
      let labelFill = null;
      let labelText = null;
      let labelFont = false;
      let fieldIndex = coordsToIndex(u, v);
      let field = state.fields[fieldIndex];
      if (field === null) {
        fieldStroke = EMPTY_FIELD_STROKE;
        fieldFill = EMPTY_FIELD_FILL;
        let scores = calculateFieldInfluence(state, u, v);
        if (scores[0] || scores[1]) {
          let score = scores[0] - scores[1];
          labelFont = '24px sans-serif'
          labelText = (score > 0 ? '+' : '') + score;
          labelFill =
              score > 0 ? EMPTY_RED_LABEL_FILL :
              score < 0 ? EMPTY_BLUE_LABEL_FILL : EMPTY_LABEL_FILL;
        } else {
          labelFill = EMPTY_LABEL_FILL;
          labelFont = '32px sans-serif'
          labelText = coordsToString(u, v);
        }
      } else if (field == 0) {
        fieldStroke = BROWN_FIELD_STROKE;
        fieldFill = BROWN_FIELD_FILL;
      } else if (field > 0) {
        fieldStroke = RED_FIELD_STROKE;
        fieldFill = RED_FIELD_FILL;
        labelFont = 'bold 32px sans-serif'
        labelFill = RED_LABEL_FILL;
        labelText = field.toString();
      } else {
        fieldStroke = BLUE_FIELD_STROKE;
        fieldFill = BLUE_FIELD_FILL;
        labelFont = 'bold 32px sans-serif'
        labelFill = BLUE_LABEL_FILL;
        labelText = (-field).toString();
      }
      let coords = boardCoords(u, v);
      context.beginPath();
      context.lineWidth = 4;
      context.arc(coords.x, coords.y, SCALE - 2, 0, TAU);
      context.fillStyle = fieldFill;
      context.fill();
      context.strokeStyle = fieldStroke;
      context.stroke();
      if (labelText !== null) {
        context.font = labelFont;
        context.textAlign = 'center';
        context.textBaseline = 'middle';
        context.fillStyle = labelFill;
        context.fillText(labelText, coords.x, coords.y);
      }
      if (fieldIndex === state.lastMove) {
        context.lineWidth = 4;
        context.strokeStyle = labelFill;
        context.beginPath();
        context.arc(coords.x, coords.y, SCALE - 8, 0, TAU);
        context.stroke();
      }
    }
  }
}

function updateStatusMessage(statusElem, state) {
  var statusMessage;
  if (state.nextPlayer === 0) {
    statusMessage = 'Waiting for initial stones.';
  } else if (state.nextPlayer === +1) {
    statusMessage = 'Red to move.';
  } else if (state.nextPlayer === -1) {
    statusMessage = 'Blue to move.';
  } else {
    var score = calculateScore(state);
    statusMessage = 'Result: ' + (score > 0 ? '+' + score : score) + '. ' +
        (score > 0 ? 'Red wins!' : score < 0 ? 'Blue wins!' : "It's a tie!");
  }
  replaceText(statusElem, statusMessage);
}

function removeChildren(elem) {
  while (elem.firstChild) elem.removeChild(elem.firstChild);
}

function replaceText(elem, text) {
  removeChildren(elem);
  elem.appendChild(document.createTextNode(text));
}

function updateMovesTable(state) {
  if (!globalMovesTableCells) return;
  for (let i = 0; i < globalMovesTableCells.length; ++i) {
    let td = globalMovesTableCells[i];
    if (i < state.history.length - INITIAL_STONES) {
      var move = state.history[i + INITIAL_STONES];
      replaceText(td, moveToString(move));
    } else {
      removeChildren(td);
    }
  }
}

function updateState(newState) {
  globalState = newState;
  drawBoard(boardCanvas, globalState);
  setSelectedPiece(globalSelectedPiece);
  updateStatusMessage(statusElem, globalState);
  if (stateStringElem) replaceText(stateStringElem, encodeState(newState));
  updateMovesTable(newState);
}

let boardCanvas = document.getElementById('board');
let piecesCanvas = document.getElementById('pieces');
let statusElem = document.getElementById('status');
let stateStringElem = document.getElementById('stateString');
let movesTable = document.getElementById('moves');
let globalMovesTableCells = null;

if (boardCanvas) {
  boardCanvas.addEventListener('click', function(event) {
    var rect = this.getBoundingClientRect();
    var x = event.clientX - rect.left;
    var y = event.clientY - rect.top;
    for (let u = 0; u < SIZE; ++u) {
      for (let v = 0; u + v < SIZE; ++v) {
        let coords = boardCoords(u, v);
        if (Math.hypot(coords.x - x, coords.y - y) < SCALE) {
          this.dispatchEvent(new CustomEvent('fieldClick', {detail: {u: u, v: v}}));
        }
      }
    }
  });

  // Hack to stop clicks on the canvas from causing surrounding text to be selected.
  boardCanvas.onmousedown = function() {return false};
}

if (piecesCanvas) {
  piecesCanvas.addEventListener('click', function(event) {
    var rect = this.getBoundingClientRect();
    var x = event.clientX - rect.left;
    var y = event.clientY - rect.top;
    var r = Math.floor(y/(SCALE*1.6));
    var c = Math.floor(x/(SCALE*1.6));
    var value = 5*r + c + 1;
    if (value > MAX_VALUE) value = MAX_VALUE - value;
    this.dispatchEvent(new CustomEvent('pieceClick', {detail: {value: value}}));
  });

  // Hack to stop clicks on the canvas from causing surrounding text to be selected.
  piecesCanvas.onmousedown = function() {return false};
}

if (movesTable) {
  let elements = movesTable.getElementsByTagName('tbody');
  if (elements.length > 0) {
    var tbody = elements[0];
    globalMovesTableCells = [];
    removeChildren(tbody);
    for (var i = 0; i < 15; ++i) {
      var tr = document.createElement('tr');
      var th = document.createElement('th');
      th.appendChild(document.createTextNode(i + 1));
      tr.appendChild(th);
      for (var j = 0; j < 2; ++j) {
        var td = document.createElement('td');
        globalMovesTableCells.push(td);
        tr.appendChild(td);
      }
      tbody.append(tr);
    }
  }
}
