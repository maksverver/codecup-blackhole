(function(){

let stateIdInput = document.getElementById('joinStateId');
let joinButton = document.getElementById('joinButton');

joinButton.addEventListener('click', function() {
  let stateId = stateIdInput.value;
  if (!stateId) {
    alert('Please enter a state ID or URL!');
    return;
  }
  stateId = stateId.substr(stateId.lastIndexOf('/') + 1);
  stateId = stateId.substr(stateId.lastIndexOf('#') + 1);
  window.location.href = BASE_URL + '/play.html#' + stateId;
});

stateIdInput.addEventListener('keyup', function(event) {
  // When hitting enter, simulate clicking the "create" button.
  if (event.keyCode == 13) joinButton.click();
});

})();
