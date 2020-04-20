(function(){

let stateStringInput = document.getElementById('analyzeStateString');
let analyzeButton = document.getElementById('analyzeButton');

analyzeButton.addEventListener('click', function() {
  let stateString = stateStringInput.value;
  if (!stateString) {
    alert('Please enter a state string!');
    return;
  }
  if (!decodeState(stateString)) {
    alert('Given state string is invalid!');
    return;
  }
  window.location.href = BASE_URL + '/analyze.html#' + stateString;
});

stateStringInput.addEventListener('keyup', function(event) {
  // When hitting enter, simulate clicking the "create" button.
  if (event.keyCode == 13) analyzeButton.click();
});

})();
