let globalStateId = stripHash(window.location.hash || "");
let globalBaseUrl = window.location.href.substr(0, window.location.href.lastIndexOf('/'));
let globalStateUrl = globalBaseUrl + '/states/' + globalStateId;
let globalLastETag = null;

function stripHash(s) {
  return s.startsWith('#') ? s.substr(1) : s;
}

function pollForChange(etag) {
  if (!etag) {
    alert('Missing ETag response header! Long polling disabled.');
    return;
  }
  globalLastETag = etag;
  let request = new XMLHttpRequest();
  request.addEventListener('error', function() {
    alert('XMLHttpRequest failed!');
  });
  request.addEventListener('load', function() {
    if (request.status == 304) {  // Not Modified
      pollForChange(etag);
    } else if (request.status != 200) {
      alert('Game retrieval request failed!\n' +
        'Server returned status code: ' + request.status + '.\n' +
        'Reload the page to continue.');
    } else {
      let stateString = request.responseText;
      let newState = decodeState(stateString);
      if (!newState) {
        alert('Invalid state string: ' + JSON.stringify(stateString));
      } else {
        updateState(newState);
      }
      pollForChange(request.getResponseHeader('ETag'));
    }
  });
  request.open('GET', globalStateUrl);
  request.setRequestHeader('If-None-Match', etag);
  request.send();
}

function setInitialStateString(stateString) {
  let initialState = decodeState(stateString);
  if (!initialState) {
    alert('Invalid state string: ' + JSON.stringify(stateString));
    return;
  }

  updateState(initialState);

  boardCanvas.addEventListener('fieldClick', function(event) {
    let u = event.detail.u;
    let v = event.detail.v;
    let newState = doMove(globalState, u, v, globalSelectedPiece);
    if (newState == null) {
      console.log('Invalid move: ' + coordsToString(u, v) + '=' + globalSelectedPiece);
      return;
    }

    let request = new XMLHttpRequest();
    request.addEventListener('error', function() {
      alert('XMLHttpRequest failed!');
    });
    request.addEventListener('load', function() {
      if (request.status == 412) {
        alert('Game state has changed! Wait for the new state, and try again?');
      } else if (request.status != 200) {
        alert('Game retrieval request failed!\n' +
          'Server returned status code: ' + request.status + '.\n' +
          'Reload the page to continue.');
      } else {
        // Nothing to be done. We should be polling for state changes, and
        // will receive the update that way.
      }
    });
    request.open('PUT', globalStateUrl);
    request.setRequestHeader('If-Match', globalLastETag);
    request.send(encodeState(newState));
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
      pollForChange(request.getResponseHeader('ETag'));
    }
  });
  request.open('GET', globalStateUrl);
  request.send();
}

initialize();
