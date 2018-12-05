#!/usr/bin/python
import sys
import os
import random
import types
import json
import hashlib
import struct
import Queue
from threading import *

g_debug = True  # Must be defined before local imports.

from net_common import *
from data_common import *
from packets import *
from world import *

HERE = os.path.dirname(os.path.abspath(__file__))

g_active_thread_count = 0
g_config = None
g_world = None

class HandlerThread(Thread):
  def __init__(self, s):
    Thread.__init__(self)
    self.s = s
    self.active = True  # Soft flag for checking whether this sessions is still
                        # alive.

    self.shutdown_lock = Lock()
    self.shutdown_done = False

    self.send_thread = None

    self.player_id = None

  def force_disconnect(self):
    # Called out of thread when another connection with the same player_id
    # connects. Note that both the thread and the socket might have already
    # been closed at this point.

    # If the thread is still running, a force disconnect will stop it in
    # its tracks at the next packet acquisition.

    if not self.active:
      return  # Nothing to do.
    self.active = False

    # Don't call shutdown twice - it might actually shutdown another player's
    # connection.
    with self.shutdown_lock:
      if self.shutdown_done:
        return

      try:
        self.s.shutdown(socket.SHUT_RDWR)
      except socket.error as e:
        if e.errno == 107:  # Client already disconnected.
          pass
        else:
          print "Socket (Sforce)", e

      self.shutdown_done = True

      # Actually do not close it, the main thread will close it after it exits
      # the worker function.
      #self.s.close()

  def run(self):
    global g_active_thread_count
    g_active_thread_count += 1

    self.s.settimeout(120)  # 2 minutes max.
    sys.stdout.write("%i " % g_active_thread_count)
    sys.stdout.flush()

    try:
      self.worker()
    except socket.timeout as e:
      pass
    except socket.error as e:
      if e.errno in {107, 32, 10053}:  # Client disconnected.
        pass
      else:
        print "Socket (W)", e
    except PacketException as e:
      if g_debug:
        print "PacketException:", e.err
    except struct.error as e:
      if g_debug:
        print "struct.error:", e

    self.active = False

    with self.shutdown_lock:
      if not self.shutdown_done:
        try:
          self.s.shutdown(socket.SHUT_RDWR)
        except socket.error as e:
          if e.errno == 107:  # Client already disconnected.
            pass
          else:
            print "Socket (S)", e
        self.shutdown_done = True

    self.s.close()  # Finally close the destructor.
    g_world.release_player_session(self.player_id, self)

    g_active_thread_count -= 1
    sys.stdout.write("%i " % g_active_thread_count)
    sys.stdout.flush()

  def post_packet(self, p):  # Called by The World.
    if self.active:
      self.send_queue.put(p)

  def sender(self):  # Running in another thread.
    while self.active:
      try:
        p = self.send_queue.get(True, 0.1)
      except Queue.Empty:
        continue  # Check it the thread is still active.

      # Send the packet.
      try:
        p.send(self.s)
      except socket.error as e:
        if g_debug:
          print "Socket (S)", e

        # This is probably know to the other thread, but...
        self.active = False
        break

  def worker(self):
    # Authenticate session.
    entr = PacketCS_ENTR(self.s)

    passwd_hash = hashlib.sha256(entr.passwd).hexdigest()
    if passwd_hash != g_config[u"passwd_hashes"][entr.player_id]:
      if g_debug:
        print "Invalid password."
      return

    # Select player.
    g_world.acquire_player_session(entr.player_id, self)
    self.player_id = entr.player_id

    if g_world.players[self.player_id] is None:
      # Request configuring the PC now.
      PacketSC_NOPC().send(self.s)
      mypc = PacketCS_MYPC(self.s)

      g_world.create_player(self.player_id, mypc.pc_name, mypc.portrait)
      # Note: create_player() might fail in case of a very unlikely race
      # condition. But if it fails, it also means that the player was
      # successfully created by another session (which is not dead), so
      # whatever.

    self.player = g_world.players[self.player_id]
    if g_debug:
      print "Player '%s' logged in." % self.player.name

    PacketSC_GAME().send(self.s)

    # Start packet sending thread.
    self.send_queue = Queue.Queue()
    th = Thread(target=self.sender)
    th.daemon = False
    th.start()  # No need to wait for thread start-up.
    self.send_thread = th

    g_world.post_event({
        "event": EV_SPECIAL,
        "type": "basic_info_request",
        "player_id": self.player_id
    })

    # Handle events.
    while self.active:
      p = PacketReceiver(self.s)
      if p.chunk_id in { "MYPC", "ENTR" }:
        raise PacketException(
            "Received packet %s after initialization phase." % p.chunk_id)

      # Handle low-level packets.
      if p.chunk_id == "GBYE":
        # Good bye!
        return

      if p.chunk_id == "PING":
        PacketSC_PONG().send(self.s)
        continue

      # Pass everything else to the world.
      g_world.post_event({
          "event": EV_PACKET,
          "player_id": self.player_id,
          "p": p
      })


def load_config():
  global g_config
  with open(os.path.join(HERE, "data", "config.json"), "rb") as f:
    g_config = json.load(f)

def main():
  print "Initializing..."
  load_config()

  print "Loading The World..."
  global g_world
  g_world = World()

  print "Starting networking..."
  s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  s.bind(("0.0.0.0", g_config["port"]))
  s.listen(123)

  print "Serving!"

  while True:
    conn, addr = s.accept()
    #print addr, "(connections active: %i)" % g_active_thread_count
    h = HandlerThread(gsocket_wrapper(conn))
    h.daemon = True
    h.start()

if __name__ == "__main__":
  sys.exit(main())

