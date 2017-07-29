let createButton = document.getElementById('createButton');
let stateStringInput = document.getElementById('stateString');
let globalBaseUrl = window.location.href.substr(0, window.location.href.lastIndexOf('/'));
let globalStatesUrl = globalBaseUrl + '/states';

function setEnabled(v) {
  createButton.disabled = !v;
  stateStringInput.disabled = !v;
}

createButton.addEventListener('click', function() {
  let stateString = stateStringInput.value;
  if (!decodeState(stateString)) {
    alert('Invalid state string: ' + JSON.stringify(stateString));
    return;
  }
  setEnabled(false);
  let request = new XMLHttpRequest();
  request.addEventListener('error', function() {
    alert('XMLHttpRequest failed!');
    setEnabled(true);
  });
  request.addEventListener('load', function() {
    if (request.status != 200) {
      alert('Game creation request failed! Server returned status code: ' + request.status);
    } else if (!request.responseText) {
      alert('Missing response text!');
    } else {
      window.location.href = globalBaseUrl + '/play.html#' + request.responseText;
    }
    setEnabled(true);
  });
  request.open('POST', globalStatesUrl);
  request.setRequestHeader('Content-Type', 'text/plain');
  request.send(stateString);
});

stateStringInput.addEventListener('keyup', function(event) {
  // When hitting enter, simulate clicking the "create" button.
  if (event.keyCode == 13) createButton.click();
});
