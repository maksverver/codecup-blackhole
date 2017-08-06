#!/usr/bin/env python

# Tool to connect a CAIA-compatible blackhole player program to the web.
#
# Note: this tool does not verify game states or moves are valid!
#
# See http://www.codecup.nl/rules_blackhole.php for the CAIA player protocol.
# See ../server/server.txt for the web protocol.

import random
import subprocess
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
REQUEST_DELAY = 5.0  # minimum time between GET requests, in seconds
COLORS = ('red', 'blue')
FIELDS = [chr(ord('A') + u) + str(v + 1) for u in range(8) for v in range(8 - u)]
BASE36DIGITS = '0123456789abcdefghijklmnopqrstuvwxyz'
INITIAL_STONES = 5
MAX_VALUE = 15
assert len(FIELDS) == len(BASE36DIGITS) == INITIAL_STONES + 2*MAX_VALUE + 1 == 36

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

def ParsePlayerMove(s):
  '''Parses a move of the form A1=1, and returns it as (field_index, value), or
     returns None if the move is not formatted correctly.'''
  if s.count('=') != 1:
    return None
  field, value = s.split('=')
  try:
    field = FIELDS.index(field)
    value = int(value)
  except ValueError:
    return None
  if value < 1 or value > MAX_VALUE:
    return None
  return (field, value)

def ParseCompactMove(s, move_index):
  assert len(s) == 2
  field = BASE36DIGITS.index(s[0])
  value = BASE36DIGITS.index(s[1])
  move_index -= INITIAL_STONES
  if move_index >= 0:
    value -= move_index%2*MAX_VALUE
  return (field, value)

def PlayGame(state_url, color, player):
  my_turn = COLORS.index(color)
  proc = subprocess.Popen(player, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  def WriteLine(s):
    proc.stdin.write(s + '\n')
    proc.stdin.flush()
  def ReadLine():
    return proc.stdout.readline().strip()
  state_string = None
  etag = None
  moves_sent = 0
  my_moves_played = 0
  moves_played = 0
  my_last_move = None
  while moves_played < INITIAL_STONES + 2*MAX_VALUE - 1:
    previous_state_string = state_string or ''
    state_string, etag = GetNextState(state_url, state_string, etag)
    assert state_string.startswith(previous_state_string)
    assert len(state_string)%2 == 0
    moves_played = len(state_string)//2
    if moves_played > INITIAL_STONES + my_moves_played*2 + my_turn:
      print 'Invalid game state! Too many moves have been played.'
      break
    while moves_sent < moves_played:
      encoded_move = state_string[2*moves_sent:2*moves_sent + 2]
      field, value = ParseCompactMove(encoded_move, moves_sent)
      if moves_sent < INITIAL_STONES:
        assert value == 0
        WriteLine(FIELDS[field])
      elif (moves_sent - INITIAL_STONES)%2 == my_turn:
        # No need to send the player's own moves back to the player program.
        assert encoded_move == my_last_move
      else:
        assert 1 <= value <= MAX_VALUE
        WriteLine(FIELDS[field] + '=' + str(value))
      moves_sent += 1
    if moves_played == INITIAL_STONES + my_moves_played*2 + my_turn:
      # It's my player's turn!
      if moves_played == INITIAL_STONES:
        WriteLine('Start')
      line = ReadLine()
      move = ParsePlayerMove(line)
      if move is None:
        print 'Player sent an invalid move:', line
        break
      my_last_move = BASE36DIGITS[move[0]] + BASE36DIGITS[move[1] + MAX_VALUE*my_turn]
      my_moves_played += 1
      PutState(state_url, state_string + my_last_move, etag)
  WriteLine('Quit')
  proc.wait()

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
