let globalStateString = null;

function stripHash(s) {
  return s.startsWith('#') ? s.substr(1) : s;
}

function updateStateString(stateString) {  
  if (stateString === globalStateString) {
    return;
  }
  let state = decodeState(stateString);
  if (!state) {
    alert('Invalid state string: ' + JSON.stringify(stateString));
    return;
  }
  
  updateState(state);

  if (globalStateString === null) {
    // First valid state. Hook up event listeners to process moves.

    boardCanvas.addEventListener('fieldClick', function(event) {
      let u = event.detail.u;
      let v = event.detail.v;
      let newState = doMove(globalState, u, v, globalSelectedPiece);
      if (newState == null) {
        console.log('Invalid move: ' + coordsToString(u, v) + '=' + globalSelectedPiece);
        return;
      } else {
        updateState(newState);
        window.location.hash = '#' + encodeState(newState);
      }
    });

    piecesCanvas.addEventListener('pieceClick', function(event) {
      let value = event.detail.value;
      setSelectedPiece(value == globalSelectedPiece ? 0 : value);
      event.stopPropagation();
    });
  }
  globalStateString = stateString;
}

window.addEventListener("hashchange", function() {
  updateStateString(stripHash(window.location.hash || ""));
});

updateStateString(stripHash(window.location.hash || ""));

