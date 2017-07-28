#!/usr/bin/python

# Quick-and-dirty HTTP server for sharing game state. See server.txt for details.

from hashlib import md5
import os
import os.path
import re
import time
import threading
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
from SimpleHTTPServer import SimpleHTTPRequestHandler
from SocketServer import ThreadingMixIn

HOST = 'localhost'
PORT = 8027

BASEDIR = os.getcwd()
WWW_DIR = os.path.join(BASEDIR, 'www')
STATES_DIR = os.path.join(BASEDIR, 'states')

STATES_PATTERN = re.compile('^/states/([0-9a-f]{20})$')
POLL_DELAY = 100  # 100 seconds (1 minute, 40 seconds)

# Used to synchronize access to states.
global_lock = threading.Lock()

# Map from state ID to State objects.
#
# Currently, entries are never removed from this dictionary, which means it
# will grow over time. We could fix that by using weak references.
states = {}

def CreateState(id, data):
  with global_lock:
    path = os.path.join(STATES_DIR, id)
    if os.path.exists(path):
      return False
    try:
      with open(path, 'w') as file:
        file.write(data)
      return True
    except IOError:
      return False

def LoadState(id):
  with global_lock:
    return LoadStateLocked(id)

def LoadStateLocked(id):
  assert global_lock.locked()
  try:
    return states[id]
  except KeyError:
    path = os.path.join(STATES_DIR, id)
    if not os.path.exists(path):
      return None
    with open(path, 'r') as file:
      data = file.read()
    new_state = states[id] = State(id, data)
    return new_state

def ParseStatesPath(path):
  m = STATES_PATTERN.match(path)
  return m.group(1) if m else None

def GenerateStateId():
  return os.urandom(10).encode('hex')

def CalculateETag(data):
  '''Generates an ETag for the given string, in the form that it should appear
  in HTTP headers. For example: "abc" (including quotes!).'''
  return '"{}"'.format(md5(data).hexdigest()[-20:])

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
  pass

class State:
  def __init__(self, id, data):
    self.id = id
    self.data = data
    self.etag = CalculateETag(data)
    self.condition = threading.Condition(global_lock)

  def Update(self, data):
    if data == self.data:
      return False
    self.data = data
    self.etag = CalculateETag(data)
    self.condition.notifyAll()
    return True

class RequestHandler(SimpleHTTPRequestHandler):
  def do_HEAD(self):
    if self.path == '/states':
      # Can't do HEAD /states (only POST).
      self.send_response(405)  # Method Not Allowed
      return
    if self.path.startswith('/states/'):
      # We could in theory implement head requests for individual states, but
      # that's currently not supported.
      self.send_response(405)  # Method NoGt Allowed
      return
    # Defer to static file.
    return SimpleHTTPRequestHandler.do_HEAD(self)

  def do_GET(self):
    if self.path == '/states':
      # Can't do GET /states (only POST).
      self.send_response(405)  # Method Not Allowed
      return
    if self.path.startswith('/states/'):
      state_id = ParseStatesPath(self.path)
      if not state_id:
        self.send_response(400)  # Bad Request
        return
      if_none_match = self.headers.getheader('If-None-Match')
      with global_lock:
        state = LoadStateLocked(state_id)
        if not state:
          self.send_response(404)  # Not Found
          return
        if if_none_match:
          if state.etag == if_none_match:
            state.condition.wait(POLL_DELAY)
          if state.etag == if_none_match:
            self.send_response(304)  # Not Modified
            return
        self.send_response(200)  # OK
        self.send_header('Content-Type', 'text/plain')
        self.send_header('ETag', state.etag)
        self.end_headers()
        self.wfile.write(state.data)
      return
    # Defer to static file.
    return SimpleHTTPRequestHandler.do_GET(self)

  def do_PUT(self):
    # Can only PUT /states/xxx
    if not self.path.startswith('/states/'):
      self.send_response(405)  # Method Not Allowed
      return
    state_id = ParseStatesPath(self.path)
    if not state_id:
      self.send_response(400)  # Bad Request
      return
    if_match = self.headers.getheader('If-Match')
    if not if_match:
      self.send_response(400)  # Bad Request
      return
    state_data = self.ReadRequestBody()
    with global_lock:
      print 
      state = LoadStateLocked(state_id)
      if not state:
        self.send_response(404)  # Not Found
        return
      if state.etag != if_match:
        self.send_response(412)  # Precondition Failed
        return
      if state.Update(state_data):
        state_path = os.path.join(STATES_DIR, state_id)
        try:
          with open(state_path, 'w') as file:
            file.write(state_data)
        except IOError:
          self.send_response(500)  # Internal Server Error
          return
    self.send_response(200)  # OK
    return

  def do_POST(self):
    # Can only PUT /states
    if self.path != '/states':
      self.send_response(405)  # Method Not Allowed
      return
    state_data = self.ReadRequestBody()
    state_id = GenerateStateId()
    if not CreateState(state_id, state_data):
      # This will never happen if we generate sufficiently long and random ids.
      self.send_response(500)  # Internal Server Error
      return
    self.send_response(200)  # OK
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()
    self.wfile.write(state_id)
    return

  def ReadRequestBody(self):
    content_length = int(self.headers.getheader('Content-Length'))
    return self.rfile.read(content_length)

if __name__ == '__main__':
  # Change to www subdirectory, from which static content will be served.
  os.chdir(WWW_DIR)

  # Create states directory (if it doesn't already exist). Do this after the
  # chdir above, so that if the script is started from the wrong directory, we
  # don't create a states subdirectory in it.
  if not os.path.isdir(STATES_DIR):
    os.mkdir(STATES_DIR)

  httpd = ThreadedHTTPServer((HOST, PORT), RequestHandler)
  print 'HTTP server listening at {}:{}'.format(HOST, PORT)
  httpd.serve_forever()
