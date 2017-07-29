let globalStateId = stripHash(window.location.hash || "");
let globalBaseUrl = window.location.href.substr(0, window.location.href.lastIndexOf('/'));
let globalStateUrl = globalBaseUrl + '/states/' + globalStateId;

function stripHash(s) {
  return s.startsWith('#') ? s.substr(1) : s;
}

function setInitialStateString(stateString) {
  let initialState = decodeState(stateString);
  if (!initialState) {
    alert('Invalid state string: ' + JSON.stringify(stateString));
    return;
  }

  updateState(initialState);

  // TODO
  // TODO
  // TODO
  // Start long polling for state changes.
  // TODO
  // TODO
  // TODO

  boardCanvas.addEventListener('fieldClick', function(event) {
    let u = event.detail.u;
    let v = event.detail.v;
    let newState = doMove(globalState, u, v, globalSelectedPiece);
    if (newState == null) {
      console.log('Invalid move: ' + coordsToString(u, v) + '=' + globalSelectedPiece);
      return;
    }
    // TODO
    // TODO
    // TODO
    alert('TODO -- PUT request');
    // PUT updated state to server.
    // TODO
    // TODO
    // TODO
  });

  piecesCanvas.addEventListener('pieceClick', function(event) {
    let value = event.detail.value;
    setSelectedPiece(value == globalSelectedPiece ? 0 : value);
    event.stopPropagation();
  });
}

function initialize() {
  if (!globalStateId) {
    alert('Missing state id!');
    return;
  }

  let request = new XMLHttpRequest();
  request.addEventListener('error', function() {
    alert('XMLHttpRequest failed!');
  });
  request.addEventListener('load', function() {
    if (request.status != 200) {
      alert('Game retrieval request failed!\n' +
        'Server returned status code: ' + request.status + '.\n' + 
        'Reload the page to continue.');
    } else {
      setInitialStateString(request.responseText);
    }
  });
  request.open('GET', globalStateUrl);
  request.setRequestHeader('Content-Type', 'text/plain');
  request.send();
}

initialize();
