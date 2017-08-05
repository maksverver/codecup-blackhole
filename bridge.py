#!/usr/bin/env python

# Tool to connect a CAIA-compatible blackhole player program to the web.
#
# Note: this tool does not verify game states or moves are valid!
#
# See http://www.codecup.nl/rules_blackhole.php for the CAIA player protocol.
# See ../server/server.txt for the web protocol.

import random
import sys
import time
import urllib2

USAGE = '''Usage:

  bridge.py create http://localhost:8027/states

    Creates a new game state using the given states URL.
    Prints the state URL of the newly created game state.
    This can then be used to connect one or more players.

  bridge.py play http://localhost:8027/states/xxxx color player [args]

    Runs the given player command and interacts with the game state at the given
    state URL. The game state must not have any moves played by this player yet
    (though if this player is blue, red may already have made his first move).

    color: either "red" (first player) or "blue" (second player)
    player: path to player executable
    args: additional arguments to pass to executable (optional)
'''
COLORS = ('red', 'blue')
FIELDS = [chr(ord('A') + u) + str(v + 1) for u in range(8) for v in range(8 - u)]
BASE36DIGITS = '0123456789abcdefghijklmnopqrstuvwxyz'
INITIAL_STONES = 5
REQUEST_DELAY = 5.0  # minimum time between GET requests, in seconds

def GenerateInitialStateString():
  return ''.join(
      BASE36DIGITS[i] + BASE36DIGITS[0]
      for i in random.sample(range(len(FIELDS)), INITIAL_STONES))

def CreateGame(states_url):
  state_string = GenerateInitialStateString()
  request = urllib2.Request(states_url, state_string)
  request.add_header('Content-Type', 'text/plain')
  response = urllib2.urlopen(request)
  assert response.getcode() == 200
  assert response.info().getheader('Content-Type') == 'text/plain'
  state_id = response.read().strip()
  if not state_id:
    print 'Empty response received!'
  else:
    print states_url + '/' + urllib2.quote(state_id)

def GetNextState(state_url, old_state_string = None, old_etag = None):
  while True:
    start_time = time.time()
    request = urllib2.Request(state_url)
    if old_etag:
      request.add_header('If-None-Match', old_etag)
    try:
      response = urllib2.urlopen(request)
      assert response.info().getheader('Content-Type') == 'text/plain'
      new_state_string = response.read().strip()
      new_etag = response.info().getheader('ETag')
      assert new_etag
      if new_state_string != old_state_string:
        return (new_state_string, new_etag)
      print 'Warning: state did not change!'
      old_etag = new_etag
      # Will fall through to sleep-and-retry.
    except urllib2.HTTPError as error:
      if error.code == 304:
        pass  # Not Modified
      else:
        print 'GET request failed! Server returned status: {} ({})'.format(error.code, error.reason)
    # Enforce a minimum delay between consecutive requests.
    delay = REQUEST_DELAY - (time.time() - start_time)
    if delay > 0:
      print 'Will retry in {:.3f} seconds...'.format(delay)
      time.sleep(delay)

class PutRequest(urllib2.Request):
  def get_method(self):
    return 'PUT'

def PutState(state_url, new_state_string, old_etag):
  request = PutRequest(state_url, new_state_string)
  request.add_header('Content-Type', 'text/plain')
  request.add_header('If-Match', old_etag)
  # This will throw an error if the request fails. It would be nice to retry if
  # the error is in the 5xx range. However, for now, we'll treat all errors as
  # permanent failures.
  response = urllib2.urlopen(request)
  assert response.getcode() == 200

def PlayGame(state_url, color, player):
  state_string = etag = None
  while True:
    state_string, etag = GetNextState(state_url, state_string, etag)
    # TODO: check if its our turn, if so, get move from player, and PUT new state.
    if False:
      PutState(state_url, state_string, etag)

def Main(argv):
  if len(argv) < 2:
    print 'Missing command!'
  else:
    command = argv[1]
    args = argv[2:]
    if command == 'create':
      if len(args) < 1:
        print 'Missing argument: states URL'
      elif len(args) > 1:
        print 'Too many arguments!'
      else:
        return CreateGame(args[0])
    elif command == 'play':
      if len(args) < 1:
        print 'Missing argument: state URL'
      elif len(args) < 2:
        print 'Missing argument: color to play'
      elif args[1] not in COLORS:
        print 'Unknown color: ', args[1]
      elif len(args) < 3:
        print 'Missing argument: player command'
      else:
        return PlayGame(args[0], args[1], args[2:])
    else:
      print 'Unknown command:', command
  print
  print USAGE

if __name__ == '__main__':
  Main(sys.argv)
