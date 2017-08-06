(function(){

let stateStringInput = document.getElementById('createStateString');
let createButton = document.getElementById('createButton');

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
      window.location.href = BASE_URL + '/play.html#' + request.responseText;
    }
    setEnabled(true);
  });
  request.open('POST', STATES_URL);
  request.setRequestHeader('Content-Type', 'text/plain');
  request.send(stateString);
});

stateStringInput.addEventListener('keyup', function(event) {
  // When hitting enter, simulate clicking the "create" button.
  if (event.keyCode == 13) createButton.click();
});

})();
