#!/usr/bin/python
# Server used to handle HTTP and proxy websocket<->internal protocol.
import sys
import threading
import os
import random
import types
import binascii
import hashlib
import base64
import subprocess

from net_common import *

class MyException(Exception):
  pass

class HttpException(Exception):
  pass

class HandlerThread(threading.Thread):
  def __init__(self, s):
    threading.Thread.__init__(self)
    self.s = gsocket_wrapper(s)
    self.s.settimeout(5)  # This should be pretty dynamic actually.

  def run(self):
    try:
      self.worker()
    except socket.timeout:
      pass
    except socket.error as e:
      if e.errno in {107, 32, 10053}:  # Client disconnected.
        pass
      else:
        print "Socket (W)", e
    except MyException as e:
      print "MyException: %s" % e

    # If the websocket was passed to another process, don't close the
    # connection.
    if self.s is None:
      return

    try:
      self.s.shutdown(socket.SHUT_RDWR)
    except socket.error as e:
      if e.errno == 107:  # Client already disconnected.
        pass
      else:
        print "Socket (S)", e

    self.s.close()

  def parse_http_header(self, p):
    lines = [line.strip() for line in p.splitlines() if line]
    if not lines:
      raise MyException("Empty packet")

    first_line = lines.pop(0)
    if not lines:
      raise MyException("Pretty empty packet")

    first_line = [token for token in first_line.split(" ") if token]
    if len(first_line) != 3:
      raise MyException("Weird packet")

    method, path, _ = first_line

    headers = {}
    for line in lines:
      line = [token.strip() for token in line.split(":", 1)]
      if len(line) != 2:
        print line
        raise MyException("Really weird packet")

      headers[line[0].lower()] = line[1]

    return {
        "method": method,
        "path": path,
        "headers": headers
    }

  def worker(self):
    packet = self.s.recvuntil("\r\n\r\n")
    header = self.parse_http_header(packet)
    try:
      if header["path"] == "/websocket":
        self.handle_websocket(header)
      else:
        self.handle_http(header)
    except HttpException as e:
      print "HttpException: %s" % e
      self.s.sendall(
        "HTTP/1.1 500 Nope\r\n"
        "Content-Type: text/html;charset=utf-8\r\n"
        "\r\n"
        "%s" % e)


  def handle_websocket(self, header):
    path = header["path"]
    method = header["method"]
    headers = header["headers"]

    if "cookie" not in headers:
      raise HttpException("Missing cookies!")

    cookie = headers["cookie"]
    cookies = {}
    for name, value in (x.strip().split("=", 1) for x in cookie.split(";")):
      name = name.strip()
      value = value.strip()
      cookies[name] = value

    if "player_id" not in cookies:
      raise HttpException("Missing player_id in cookies.")

    if "password" not in cookies:
      raise HttpException("Missing password in cookies.")

    if "name" not in cookies:
      raise HttpException("Missing name in cookies.")

    player_id = cookies["player_id"]
    password = cookies["password"]
    name = cookies["name"]

    try:
      player_id = int(player_id)
    except ValueError:
      raise HttpException("Invalid player_id")

    if player_id < 0 or player_id >= 256:
      raise HttpException("Invalid player_id")

    if "upgrade" not in headers:
      raise HttpException("Do you realllyyy want a websocket?")

    if headers["upgrade"] != "websocket":
      raise HttpException("Can't do that for you.")

    if "connection" not in headers:
      raise HttpException("Missing connection")

    if "upgrade" not in headers["connection"].lower():
      raise HttpException("Not upgrading connection?")

    if "sec-websocket-key" not in headers:
      raise HttpException("Missing Sec-WebSocket-Key")

    key = headers["sec-websocket-key"]

    # Don't care about Origin/Sec-WebSocket-Protocol/Sec-WebSocket-Version.

    GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
    response = hashlib.sha1(key + GUID).digest()
    response = base64.b64encode(response)

    self.s.sendall(
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "Sec-WebSocket-Protocol: game\r\n"
        "\r\n" % response)

    # Connection established. All seems fine.
    self.spawn_client(player_id, password, name)

  def spawn_client(self, player_id, password, name):
    print 'Spawning client for: %s (%i)' % (`name`, player_id)

    # Client will take over timeouts.
    self.s.settimeout(None)

    client_env = os.environ.copy()
    client_env["ARCANE_FD"] = str(self.s.fileno())
    client_env["ARCANE_PASSWD"] = str(password)
    client_env["ARCANE_NAME"] = str(name)
    client_env["ARCANE_UI_TYPE"] = "WEBSOCKET"

    client_dir = os.environ["ARCANE_CLIENT"]
    client = client_dir + '/client'

    # Spawn!
    try:
      p = subprocess.Popen(
          [ client, str(player_id) ],
          stdout=subprocess.PIPE,
          stderr=subprocess.PIPE,
          cwd=client_dir,
          env=client_env)
    except OSError:
      raise MyException("Failed to run client. Find valis/gynvael!")

    stdoutdata, stderrdata = p.communicate()

    print "Client exited."
    stderrdata = stderrdata.strip()
    stdoutdata = stdoutdata.strip()

    if stdoutdata:
      print "-- stdout log:\n%s" % stdoutdata

    if stderrdata:
      print "-- stderr log:\n%s" % stderrdata

    print "-- end of logs"


  def handle_http(self, header):
    path = header["path"]
    method = header["method"]
    headers = header["headers"]

    if not path.startswith("/"):
      raise HttpException("Weird path")

    path = path[1:]

    if "\\" in path or "/" in path or ".." in path:
      raise HttpException("Nope.")

    query = ""
    if "?" in path:
      path, query = path.split("?", 1)

    if path == "":
      path = "index.html"

    fname = "static/" + path
    try:
      with open(fname, "rb") as f:
        data = f.read()
    except OSError:
      raise HttpException("No file.")
    except IOError:
      raise HttpException("No file.")

    _, ext = os.path.splitext(fname)

    KNOWN_EXTS = {
      ".jpg": "image/jpeg",
      ".png": "image/png",
      ".ico": "image/png",
      ".html": "text/html;charset=utf-8",
      ".js": "application/javascript;charset=utf-8",
      ".css": "text/css;charset=utf-8",
    }
    mime = KNOWN_EXTS.get(ext, "application/octet-stream")

    self.s.sendall(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %i\r\n"
      "Connection: Closed\r\n"
      "Server: This python script my cat wrote\r\n"
      "\r\n" % (mime, len(data)))
    self.s.sendall(data)

def main():
  if "ARCANE_HOST" not in os.environ:
    sys.exit("error: set ARCANE_HOST to host:port of the game server")

  if "ARCANE_CLIENT" not in os.environ:
    sys.exit("error: set ARCANE_CLIENT to client *directory* (full path!)")

  print "Initializing..."

  s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  s.bind(("0.0.0.0", 8888))
  s.listen(123)

  print "Serving!"

  while True:
    conn, addr = s.accept()
    #print addr, "(connections active: %i)" % g_active_thread_count
    h = HandlerThread(conn)
    h.daemon = True
    h.start()


if __name__ == "__main__":
  sys.exit(main())

